#include <math.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "mlp_driver.h"

static const char *TAG = "MLP";

static bool workspace_ready = false;

static float *s_x = NULL; //input layer
static float *s_h0 = NULL; //hidden
static float *s_h1 = NULL;
static float *s_logits = NULL; //output

static void free_inference_workspace(void) {
	free(s_x);
	free(s_h0);
	free(s_h1);
	free(s_logits);

	s_x = NULL;
	s_h0 = NULL;
	s_h1 = NULL;
	s_logits = NULL;
	workspace_ready = false;
}

static bool try_alloc_workspace(uint32_t caps, const char *mem_name) {
	s_x = (float *)heap_caps_malloc(sizeof(float) * MLP_INPUT_SIZE, caps);
	s_h0 = (float *)heap_caps_malloc(sizeof(float) * MLP_HIDDEN0_SIZE, caps);
	s_h1 = (float *)heap_caps_malloc(sizeof(float) * MLP_HIDDEN1_SIZE, caps);
	s_logits = (float *)heap_caps_malloc(sizeof(float) * MLP_OUTPUT_SIZE, caps);

	if (s_x == NULL || s_h0 == NULL || s_h1 == NULL || s_logits == NULL) {
		free_inference_workspace();
		ESP_LOGW(TAG, "Failed to allocate MLP workspace in %s", mem_name);
		return false;
	}

	workspace_ready = true;
	ESP_LOGI(TAG, "MLP inference workspace allocated in %s", mem_name);
	return true;
}             

static bool alloc_cnn_inference_workspace(void) {
	if (workspace_ready) {
		return true;
	}
//try SPRAM first
#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
	if (try_alloc_workspace(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, "PSRAM")) {
		return true;
	}
	ESP_LOGW(TAG, "Falling back to internal RAM for MLP workspace");
#else
	ESP_LOGI(TAG, "PSRAM disabled; using internal RAM for MLP workspace");
#endif

	return try_alloc_workspace(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, "internal RAM");
}

static float relu(float value) {
	return (value > 0.0f) ? value : 0.0f;
}

static float normalize_feature(int idx, int16_t raw_value) {
	const float denom = fabsf(input_scaler_std_mlp[idx]) > 1e-9f ? input_scaler_std_mlp[idx] : 1.0f;
	return ((float)raw_value - input_scaler_mean_mlp[idx]) / denom;
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
	if (out == NULL) {
		return;
	}

	out->class_id = 0;
	out->confidence = 0.0f;
	out->motion_score = 0.0f;

	if (raw_window == NULL) {
		return;
	}

	if (!alloc_cnn_inference_workspace()) {
		ESP_LOGE(TAG, "MLP inference workspace allocation failed");
		return;
	}

	out->motion_score = compute_motion_score(raw_window, raw_len);

	for (int i = 0; i < MLP_INPUT_SIZE; i++) {
		s_x[i] = 0.0f;
	}

	for (int i = 0; i < MLP_INPUT_SIZE; i++) {
		if ((size_t)i < raw_len) {
			s_x[i] = normalize_feature(i, raw_window[i]);
		}
	}

	for (int row = 0; row < MLP_HIDDEN0_SIZE; row++) {
		float acc = mlp_b0[row];
		const float scale = mlp_w0_scale[row];
		const int base = row * MLP_INPUT_SIZE;
		for (int col = 0; col < MLP_INPUT_SIZE; col++) {
			acc += ((float)mlp_w0_int8[base + col] * scale) * s_x[col];
		}
		s_h0[row] = relu(acc);
	}

	for (int row = 0; row < MLP_HIDDEN1_SIZE; row++) {
		float acc = mlp_b1[row];
		const float scale = mlp_w1_scale[row];
		const int base = row * MLP_HIDDEN0_SIZE;
		for (int col = 0; col < MLP_HIDDEN0_SIZE; col++) {
			acc += ((float)mlp_w1_int8[base + col] * scale) * s_h0[col];
		}
		s_h1[row] = relu(acc);
	}

	int argmax = 0;
	float max_logit = -INFINITY;
	float sum_exp = 0.0f;

	for (int row = 0; row < MLP_OUTPUT_SIZE; row++) {
		float acc = mlp_b2[row];
		const float scale = mlp_w2_scale[row];
		const int base = row * MLP_HIDDEN1_SIZE;
		for (int col = 0; col < MLP_HIDDEN1_SIZE; col++) {
			acc += ((float)mlp_w2_int8[base + col] * scale) * s_h1[col];
		}
		s_logits[row] = acc;
		if (acc > max_logit) {
			max_logit = acc;
			argmax = row;
		}
	}

	for (int i = 0; i < MLP_OUTPUT_SIZE; i++) {
		sum_exp += expf(s_logits[i] - max_logit);
	}

	out->class_id = argmax;
	if (sum_exp > 0.0f) {
		out->confidence = expf(s_logits[argmax] - max_logit) / sum_exp;
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

	int len = buf->maxlen;
	if (len > MLP_INPUT_SIZE) {
		len = MLP_INPUT_SIZE;
	}

	mlp_predict_raw(buf->buffer, (size_t)len, out);
}
