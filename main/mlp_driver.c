#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "mlp_driver.h"

static float relu(float value) {
	return (value > 0.0f) ? value : 0.0f;
}

static float normalize_feature(int idx, int16_t raw_value) {
	const float denom = fabsf(input_scaler_std[idx]) > 1e-9f ? input_scaler_std[idx] : 1.0f;
	return ((float)raw_value - input_scaler_mean[idx]) / denom;
}

static float compute_motion_score(const int16_t *raw_window, size_t len) {
	if (raw_window == NULL || len <= MLP_CHANNELS) {
		return 0.0f;
	}

	float accum = 0.0f;
	int count = 0;
	for (size_t i = MLP_CHANNELS; i < len; i++) {
		const int diff = (int)raw_window[i] - (int)raw_window[i - MLP_CHANNELS];
		accum += (float)abs(diff);
		count++;
	}

	if (count == 0) {
		return 0.0f;
	}

	return accum / (float)count;
}

void mlp_predict_raw(const int16_t *raw_window, size_t raw_len, mlp_result_t *out) {
	float x[MLP_INPUT_SIZE] = {0.0f};
	float h0[MLP_HIDDEN0_SIZE] = {0.0f};
	float h1[MLP_HIDDEN1_SIZE] = {0.0f};
	float logits[MLP_OUTPUT_SIZE] = {0.0f};

	if (out == NULL) {
		return;
	}

	out->class_id = 0;
	out->confidence = 0.0f;
	out->motion_score = 0.0f;

	if (raw_window == NULL) {
		return;
	}

	out->motion_score = compute_motion_score(raw_window, raw_len);

	for (int i = 0; i < MLP_INPUT_SIZE; i++) {
		if ((size_t)i < raw_len) {
			x[i] = normalize_feature(i, raw_window[i]);
		}
	}

	for (int row = 0; row < MLP_HIDDEN0_SIZE; row++) {
		float acc = mlp_b0[row];
		const float scale = mlp_w0_scale[row];
		const int base = row * MLP_INPUT_SIZE;
		for (int col = 0; col < MLP_INPUT_SIZE; col++) {
			acc += ((float)mlp_w0_int8[base + col] * scale) * x[col];
		}
		h0[row] = relu(acc);
	}

	for (int row = 0; row < MLP_HIDDEN1_SIZE; row++) {
		float acc = mlp_b1[row];
		const float scale = mlp_w1_scale[row];
		const int base = row * MLP_HIDDEN0_SIZE;
		for (int col = 0; col < MLP_HIDDEN0_SIZE; col++) {
			acc += ((float)mlp_w1_int8[base + col] * scale) * h0[col];
		}
		h1[row] = relu(acc);
	}

	int argmax = 0;
	float max_logit = -INFINITY;
	float sum_exp = 0.0f;

	for (int row = 0; row < MLP_OUTPUT_SIZE; row++) {
		float acc = mlp_b2[row];
		const float scale = mlp_w2_scale[row];
		const int base = row * MLP_HIDDEN1_SIZE;
		for (int col = 0; col < MLP_HIDDEN1_SIZE; col++) {
			acc += ((float)mlp_w2_int8[base + col] * scale) * h1[col];
		}
		logits[row] = acc;
		if (acc > max_logit) {
			max_logit = acc;
			argmax = row;
		}
	}

	for (int i = 0; i < MLP_OUTPUT_SIZE; i++) {
		sum_exp += expf(logits[i] - max_logit);
	}

	out->class_id = argmax;
	if (sum_exp > 0.0f) {
		out->confidence = expf(logits[argmax] - max_logit) / sum_exp;
	}
}

void mlp_predict_buffer(const Circ_buf *buf, mlp_result_t *out) {
	if (buf == NULL) {
		if (out != NULL) {
			out->class_id = 0;
			out->confidence = 0.0f;
			out->motion_score = 0.0f;
		}
		return;
	}

	int16_t ordered[MLP_INPUT_SIZE] = {0};
	int len = buf->maxlen;
	if (len > MLP_INPUT_SIZE) {
		len = MLP_INPUT_SIZE;
	}

	for (int i = 0; i < len; i++) {
		const int src_idx = (buf->head + i) % buf->maxlen;
		ordered[i] = buf->buffer[src_idx];
	}

	mlp_predict_raw(ordered, (size_t)len, out);
}
