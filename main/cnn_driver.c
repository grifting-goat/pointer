#include <math.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "cnn_driver.h"

static const char *TAG = "CNN";

static bool workspace_ready = false;

static float *s_x = NULL; //input layer
static float *s_conv0 = NULL;
static float *s_conv1 = NULL;
static float *s_pool0 = NULL;
static float *s_conv2 = NULL;
static float *s_conv3 = NULL;
static float *s_gap = NULL;
static float *s_fc0 = NULL;
static float *s_logits = NULL;

static const float BN_EPS = 1e-5f;
static bool s_precompute_ready = false;

static float s_conv0_scale[CNN_CONV0_OUT_CHANNELS];
static float s_conv0_bias[CNN_CONV0_OUT_CHANNELS];
static float s_conv1_scale[CNN_CONV1_OUT_CHANNELS];
static float s_conv1_bias[CNN_CONV1_OUT_CHANNELS];
static float s_conv2_scale[CNN_CONV2_OUT_CHANNELS];
static float s_conv2_bias[CNN_CONV2_OUT_CHANNELS];
static float s_conv3_scale[CNN_CONV3_OUT_CHANNELS];
static float s_conv3_bias[CNN_CONV3_OUT_CHANNELS];
static float s_input_inv_std[CNN_INPUT_SIZE];

static void free_inference_workspace(void) {
    free(s_x);
	free(s_conv0);
	free(s_conv1);
	free(s_pool0);
	free(s_conv2);
	free(s_conv3);
	free(s_gap);
	free(s_fc0);
    free(s_logits);

    s_x = NULL;
	s_conv0 = NULL;
	s_conv1 = NULL;
	s_pool0 = NULL;
	s_conv2 = NULL;
	s_conv3 = NULL;
	s_gap = NULL;
	s_fc0 = NULL;
    s_logits = NULL;
	workspace_ready = false;
}

static void precompute_inference_constants(void) {
	if (s_precompute_ready) {
		return;
	}

	for (size_t i = 0; i < CNN_INPUT_SIZE; i++) {
		const float std = input_scaler_std[i];
		const float denom = fabsf(std) > 1e-9f ? std : 1.0f;
		s_input_inv_std[i] = 1.0f / denom;
	}

	for (size_t oc = 0; oc < CNN_CONV0_OUT_CHANNELS; oc++) {
		const float bn_mul = features_1_weight[oc] / sqrtf(features_1_running_var[oc] + BN_EPS);
		s_conv0_scale[oc] = features_0_weight_scale[oc] * bn_mul;
		s_conv0_bias[oc] = ((features_0_bias[oc] - features_1_running_mean[oc]) * bn_mul) + features_1_bias[oc];
	}
	for (size_t oc = 0; oc < CNN_CONV1_OUT_CHANNELS; oc++) {
		const float bn_mul = features_4_weight[oc] / sqrtf(features_4_running_var[oc] + BN_EPS);
		s_conv1_scale[oc] = features_3_weight_scale[oc] * bn_mul;
		s_conv1_bias[oc] = ((features_3_bias[oc] - features_4_running_mean[oc]) * bn_mul) + features_4_bias[oc];
	}
	for (size_t oc = 0; oc < CNN_CONV2_OUT_CHANNELS; oc++) {
		const float bn_mul = features_8_weight[oc] / sqrtf(features_8_running_var[oc] + BN_EPS);
		s_conv2_scale[oc] = features_7_weight_scale[oc] * bn_mul;
		s_conv2_bias[oc] = ((features_7_bias[oc] - features_8_running_mean[oc]) * bn_mul) + features_8_bias[oc];
	}
	for (size_t oc = 0; oc < CNN_CONV3_OUT_CHANNELS; oc++) {
		const float bn_mul = features_11_weight[oc] / sqrtf(features_11_running_var[oc] + BN_EPS);
		s_conv3_scale[oc] = features_10_weight_scale[oc] * bn_mul;
		s_conv3_bias[oc] = ((features_10_bias[oc] - features_11_running_mean[oc]) * bn_mul) + features_11_bias[oc];
	}

	s_precompute_ready = true;
}

static bool try_alloc_workspace(uint32_t caps, const char *mem_name) {
    s_x = (float *)heap_caps_malloc(sizeof(float) * CNN_INPUT_SIZE, caps);
	s_conv0 = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_CONV0_OUT_CHANNELS * (size_t)CNN_SEQ_LEN, caps);
	s_conv1 = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_CONV1_OUT_CHANNELS * (size_t)CNN_SEQ_LEN, caps);
	s_pool0 = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_CONV1_OUT_CHANNELS * ((size_t)CNN_SEQ_LEN / 2U), caps);
	s_conv2 = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_CONV2_OUT_CHANNELS * ((size_t)CNN_SEQ_LEN / 2U), caps);
	s_conv3 = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_CONV3_OUT_CHANNELS * ((size_t)CNN_SEQ_LEN / 2U), caps);
	s_gap = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_FC0_IN_SIZE, caps);
	s_fc0 = (float *)heap_caps_malloc(sizeof(float) * (size_t)CNN_FC0_OUT_SIZE, caps);
	s_logits = (float *)heap_caps_malloc(sizeof(float) * CNN_OUTPUT_SIZE, caps);

	if (s_x == NULL || s_conv0 == NULL || s_conv1 == NULL || s_pool0 == NULL ||
		s_conv2 == NULL || s_conv3 == NULL || s_gap == NULL || s_fc0 == NULL ||
		s_logits == NULL) {
        free_inference_workspace();
		ESP_LOGW(TAG, "Failed to allocate CNN workspace in %s", mem_name);
		return false;
    }

    workspace_ready = true;
	ESP_LOGI(TAG, "CNN inference workspace allocated in %s", mem_name);
	return true;
}

bool alloc_cnn_inference_workspace(void) {
	if (workspace_ready) {
		return true;
	}
// Prefer internal RAM for inference speed; fallback to PSRAM for capacity.
	if (try_alloc_workspace(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, "internal RAM")) {
		return true;
	}
	ESP_LOGW(TAG, "Falling back to PSRAM for CNN workspace");
#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
	if (try_alloc_workspace(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, "PSRAM")) {
		return true;
	}
#else
	ESP_LOGI(TAG, "PSRAM disabled; using internal RAM for MLP workspace");
#endif

	return false;
}

static float relu(float value) {
	return (value > 0.0f) ? value : 0.0f;
}

static void relu_inplace(float *x, size_t len) {
	for (size_t i = 0; i < len; i++) {
		x[i] = relu(x[i]);
	}
}

static float normalize_feature(int idx, int16_t raw_value) {
	if (idx < 0 || idx >= input_scaler_len) {
		return (float)raw_value;
	}
	return ((float)raw_value - input_scaler_mean[idx]) * s_input_inv_std[idx];
}

static float compute_motion_score(const int16_t *raw_window, size_t len) {
	if (raw_window == NULL || len <= CNN_CHANNELS) {
		return 0.0f;
	}

	float accum = 0.0f;
	int count = 0;
	for (size_t i = CNN_CHANNELS; i < len; i++) {
		const int diff = (int)raw_window[i] - (int)raw_window[i - CNN_CHANNELS];
		accum += (float)abs(diff);
		count++;
	}

	if (count == 0) {
		return 0.0f;
	}

	return accum / (float)count;
}

static bool conv1d_q_f32(const float *in,
						 size_t in_ch,
						 size_t in_len,
						 const int8_t *w_q,
						 const float *w_scale,
						 const float *bias,
						 size_t out_ch,
						 size_t k_len,
						 size_t padding,
						 float *out,
						 size_t out_len) {
	if (in == NULL || w_q == NULL || w_scale == NULL || bias == NULL || out == NULL ||
		in_ch == 0U || in_len == 0U || out_ch == 0U || k_len == 0U) {
		return false;
	}

	const size_t padded_len = in_len + (padding * 2U);
	if (padded_len < k_len) {
		return false;
	}
	if ((padded_len - k_len + 1U) != out_len) {
		return false;
	}

	for (size_t oc = 0; oc < out_ch; oc++) {
		const float scale = w_scale[oc];
		for (size_t t = 0; t < out_len; t++) {
			float acc = bias[oc];
			const size_t k_start = (t < padding) ? (padding - t) : 0U;
			const size_t k_end_bound = in_len + padding - t;
			const size_t k_end = (k_end_bound < k_len) ? k_end_bound : k_len;
			for (size_t ic = 0; ic < in_ch; ic++) {
				const size_t in_base = ic * in_len;
				const size_t w_base = (oc * in_ch + ic) * k_len;
				for (size_t k = k_start; k < k_end; k++) {
					const size_t in_t = t + k - padding;
					acc += ((float)w_q[w_base + k] * scale) * in[in_base + in_t];
				}
			}
			out[oc * out_len + t] = acc;
		}
	}

	return true;
}

static void maxpool1d_2s2(const float *in, size_t channels, size_t in_len, float *out) {
	const size_t out_len = in_len / 2U;
	for (size_t c = 0; c < channels; c++) {
		const size_t in_base = c * in_len;
		const size_t out_base = c * out_len;
		for (size_t t = 0; t < out_len; t++) {
			const float a = in[in_base + (t * 2U)];
			const float b = in[in_base + (t * 2U) + 1U];
			out[out_base + t] = (a > b) ? a : b;
		}
	}
}

static void adaptive_avg_pool1d_1(const float *in, size_t channels, size_t in_len, float *out) {
	const float denom = (in_len > 0U) ? (1.0f / (float)in_len) : 0.0f;
	for (size_t c = 0; c < channels; c++) {
		const size_t base = c * in_len;
		float sum = 0.0f;
		for (size_t t = 0; t < in_len; t++) {
			sum += in[base + t];
		}
		out[c] = sum * denom;
	}
}

static bool linear_q_f32(const float *in,
						 size_t in_len,
						 const int8_t *w_q,
						 const float *w_scale,
						 const float *bias,
						 size_t out_len,
						 float *out) {
	if (in == NULL || w_q == NULL || w_scale == NULL || bias == NULL || out == NULL) {
		return false;
	}

	for (size_t o = 0; o < out_len; o++) {
		float acc = bias[o];
		const float scale = w_scale[o];
		const size_t w_base = o * in_len;
		for (size_t i = 0; i < in_len; i++) {
			acc += ((float)w_q[w_base + i] * scale) * in[i];
		}
		out[o] = acc;
	}
	return true;
}

bool conv1d(const int16_t *raw_window, size_t raw_len, const int16_t *kern, size_t kern_len, uint16_t padding, float* out, size_t out_len) {
    if (raw_window == NULL || kern == NULL || out == NULL || kern_len == 0U) {
		return false;
	}
	const size_t padded_len = raw_len + ((size_t)padding * 2U);
	if (padded_len < kern_len) {
		return false;
	}

	const size_t post_len = padded_len - kern_len + 1U;
	if (post_len > raw_len) {
		return false;
	}

    if (post_len != out_len) {return false;} // needed if i go back and correctly implement padded_len < kern_len and post_len > raw_len case

	for (size_t out_i = 0; out_i < post_len; out_i++) {
		float acc = 0.0f; 
		for (size_t k = 0; k < kern_len; k++) {
			const ptrdiff_t in_idx = (ptrdiff_t)out_i + (ptrdiff_t)k - (ptrdiff_t)padding;
			if (in_idx < 0 || (size_t)in_idx >= raw_len) {
				continue;
			}
			acc += (float)raw_window[in_idx] * (float)kern[k];
		}
		out[out_i] = acc;
	}

	return true;
}


void cnn_predict_raw(const int16_t *raw_window, size_t raw_len, cnn_result_t *out) {
    if (out == NULL) {
		return;
	}

    if (!alloc_cnn_inference_workspace()) {
		ESP_LOGE(TAG, "CNN inference workspace allocation failed");
		return;
	}

	out->class_id = 0;
	out->confidence = 0.0f;
	out->motion_score = 0.0f;

	if (raw_window == NULL) {
		return;
	}

	precompute_inference_constants();

    out->motion_score = compute_motion_score(raw_window, raw_len);

	for (size_t i = 0; i < CNN_INPUT_SIZE; i++) {
		s_x[i] = 0.0f;
	}

	const size_t usable_len = (raw_len < CNN_INPUT_SIZE) ? raw_len : CNN_INPUT_SIZE;
	for (size_t flat = 0; flat < usable_len; flat++) {
		const size_t t = flat / CNN_CHANNELS;
		const size_t c = flat % CNN_CHANNELS;
		if (t >= CNN_SEQ_LEN) {
			break;
		}
		s_x[c * CNN_SEQ_LEN + t] = normalize_feature((int)flat, raw_window[flat]);
	}

	if (!conv1d_q_f32(s_x,
					  CNN_CONV0_IN_CHANNELS,
					  CNN_SEQ_LEN,
					  features_0_weight_q,
					  s_conv0_scale,
					  s_conv0_bias,
					  CNN_CONV0_OUT_CHANNELS,
					  CNN_CONV0_KERNEL_SIZE,
					  CNN_CONV0_PADDING,
					  s_conv0,
					  CNN_SEQ_LEN)) {
		ESP_LOGE(TAG, "Conv0 failed");
		return;
	}
	relu_inplace(s_conv0, (size_t)CNN_CONV0_OUT_CHANNELS * (size_t)CNN_SEQ_LEN);

	if (!conv1d_q_f32(s_conv0,
					  CNN_CONV1_IN_CHANNELS,
					  CNN_SEQ_LEN,
					  features_3_weight_q,
					  s_conv1_scale,
					  s_conv1_bias,
					  CNN_CONV1_OUT_CHANNELS,
					  CNN_CONV1_KERNEL_SIZE,
					  CNN_CONV1_PADDING,
					  s_conv1,
					  CNN_SEQ_LEN)) {
		ESP_LOGE(TAG, "Conv1 failed");
		return;
	}
	relu_inplace(s_conv1, (size_t)CNN_CONV1_OUT_CHANNELS * (size_t)CNN_SEQ_LEN);

	maxpool1d_2s2(s_conv1, CNN_CONV1_OUT_CHANNELS, CNN_SEQ_LEN, s_pool0);
	const size_t pooled_len = (size_t)CNN_SEQ_LEN / 2U;

	if (!conv1d_q_f32(s_pool0,
					  CNN_CONV2_IN_CHANNELS,
					  pooled_len,
					  features_7_weight_q,
					  s_conv2_scale,
					  s_conv2_bias,
					  CNN_CONV2_OUT_CHANNELS,
					  CNN_CONV2_KERNEL_SIZE,
					  CNN_CONV2_PADDING,
					  s_conv2,
					  pooled_len)) {
		ESP_LOGE(TAG, "Conv2 failed");
		return;
	}
	relu_inplace(s_conv2, (size_t)CNN_CONV2_OUT_CHANNELS * pooled_len);

	if (!conv1d_q_f32(s_conv2,
					  CNN_CONV3_IN_CHANNELS,
					  pooled_len,
					  features_10_weight_q,
					  s_conv3_scale,
					  s_conv3_bias,
					  CNN_CONV3_OUT_CHANNELS,
					  CNN_CONV3_KERNEL_SIZE,
					  CNN_CONV3_PADDING,
					  s_conv3,
					  pooled_len)) {
		ESP_LOGE(TAG, "Conv3 failed");
		return;
	}
	relu_inplace(s_conv3, (size_t)CNN_CONV3_OUT_CHANNELS * pooled_len);

	adaptive_avg_pool1d_1(s_conv3, CNN_CONV3_OUT_CHANNELS, pooled_len, s_gap);

	if (!linear_q_f32(s_gap,
					  CNN_FC0_IN_SIZE,
					  classifier_2_weight_q,
					  classifier_2_weight_scale,
					  classifier_2_bias,
					  CNN_FC0_OUT_SIZE,
					  s_fc0)) {
		ESP_LOGE(TAG, "FC0 failed");
		return;
	}
	relu_inplace(s_fc0, CNN_FC0_OUT_SIZE);

	if (!linear_q_f32(s_fc0,
					  CNN_FC1_IN_SIZE,
					  classifier_5_weight_q,
					  classifier_5_weight_scale,
					  classifier_5_bias,
					  CNN_FC1_OUT_SIZE,
					  s_logits)) {
		ESP_LOGE(TAG, "FC1 failed");
		return;
	}

    int argmax = 0;
	float max_logit = s_logits[0];
	float sum_exp = 0.0f;

	for (int i = 1; i < CNN_OUTPUT_SIZE; i++) {
		if (s_logits[i] > max_logit) {
			max_logit = s_logits[i];
			argmax = i;
		}
	}


    for (int i = 0; i < CNN_OUTPUT_SIZE; i++) {
		sum_exp += expf(s_logits[i] - max_logit);
	}

	out->class_id = argmax;
	if (sum_exp > 0.0f) {
		out->confidence = expf(s_logits[argmax] - max_logit) / sum_exp;
	}

}

void cnn_predict_buffer(const Circ_buf *buf, cnn_result_t *out) {
    if (buf == NULL) {
		if (out != NULL) {
			out->class_id = 0;
			out->confidence = 0.0f;
			out->motion_score = 0.0f;
		}
		return;
	}

	int len = buf->maxlen;
	if (len > CNN_INPUT_SIZE) {
		len = CNN_INPUT_SIZE;
	}

    cnn_predict_raw(buf->buffer, (size_t)len, out);
}