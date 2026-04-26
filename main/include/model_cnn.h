#ifndef MODEL_CNN_H
#define MODEL_CNN_H

#include <stdint.h>

extern const int features_0_weight_ndim;
extern const int features_0_weight_shape[3];
extern const int features_0_weight_len;
extern const int8_t features_0_weight_q[2688];
extern const float features_0_weight_scale[64];

extern const int features_0_bias_len;
extern const float features_0_bias[64];

extern const int features_1_weight_len;
extern const float features_1_weight[64];

extern const int features_1_bias_len;
extern const float features_1_bias[64];

extern const int features_1_running_mean_len;
extern const float features_1_running_mean[64];

extern const int features_1_running_var_len;
extern const float features_1_running_var[64];

extern const int features_1_num_batches_tracked_len;
extern const float features_1_num_batches_tracked[1];

extern const int features_3_weight_ndim;
extern const int features_3_weight_shape[3];
extern const int features_3_weight_len;
extern const int8_t features_3_weight_q[40960];
extern const float features_3_weight_scale[128];

extern const int features_3_bias_len;
extern const float features_3_bias[128];

extern const int features_4_weight_len;
extern const float features_4_weight[128];

extern const int features_4_bias_len;
extern const float features_4_bias[128];

extern const int features_4_running_mean_len;
extern const float features_4_running_mean[128];

extern const int features_4_running_var_len;
extern const float features_4_running_var[128];

extern const int features_4_num_batches_tracked_len;
extern const float features_4_num_batches_tracked[1];

extern const int features_7_weight_ndim;
extern const int features_7_weight_shape[3];
extern const int features_7_weight_len;
extern const int8_t features_7_weight_q[98304];
extern const float features_7_weight_scale[256];

extern const int features_7_bias_len;
extern const float features_7_bias[256];

extern const int features_8_weight_len;
extern const float features_8_weight[256];

extern const int features_8_bias_len;
extern const float features_8_bias[256];

extern const int features_8_running_mean_len;
extern const float features_8_running_mean[256];

extern const int features_8_running_var_len;
extern const float features_8_running_var[256];

extern const int features_8_num_batches_tracked_len;
extern const float features_8_num_batches_tracked[1];

extern const int features_10_weight_ndim;
extern const int features_10_weight_shape[3];
extern const int features_10_weight_len;
extern const int8_t features_10_weight_q[196608];
extern const float features_10_weight_scale[256];

extern const int features_10_bias_len;
extern const float features_10_bias[256];

extern const int features_11_weight_len;
extern const float features_11_weight[256];

extern const int features_11_bias_len;
extern const float features_11_bias[256];

extern const int features_11_running_mean_len;
extern const float features_11_running_mean[256];

extern const int features_11_running_var_len;
extern const float features_11_running_var[256];

extern const int features_11_num_batches_tracked_len;
extern const float features_11_num_batches_tracked[1];

extern const int classifier_2_weight_ndim;
extern const int classifier_2_weight_shape[2];
extern const int classifier_2_weight_len;
extern const int8_t classifier_2_weight_q[32768];
extern const float classifier_2_weight_scale[128];

extern const int classifier_2_bias_len;
extern const float classifier_2_bias[128];

extern const int classifier_5_weight_ndim;
extern const int classifier_5_weight_shape[2];
extern const int classifier_5_weight_len;
extern const int8_t classifier_5_weight_q[256];
extern const float classifier_5_weight_scale[2];

extern const int classifier_5_bias_len;
extern const float classifier_5_bias[2];

extern const int input_scaler_len;
extern const float input_scaler_mean[648];
extern const float input_scaler_std[648];

#define CNN_INPUT_SIZE 648
#define CNN_OUTPUT_SIZE 2
#define CNN_CHANNELS 6
#define CNN_SEQ_LEN 108
#define CNN_CONV_LAYER_COUNT 4
#define CNN_FC_LAYER_COUNT 2

#define CNN_CONV0_IN_CHANNELS 6
#define CNN_CONV0_OUT_CHANNELS 64
#define CNN_CONV0_KERNEL_SIZE 7
#define CNN_CONV0_PADDING 3

#define CNN_CONV1_IN_CHANNELS 64
#define CNN_CONV1_OUT_CHANNELS 128
#define CNN_CONV1_KERNEL_SIZE 5
#define CNN_CONV1_PADDING 2

#define CNN_CONV2_IN_CHANNELS 128
#define CNN_CONV2_OUT_CHANNELS 256
#define CNN_CONV2_KERNEL_SIZE 3
#define CNN_CONV2_PADDING 1

#define CNN_CONV3_IN_CHANNELS 256
#define CNN_CONV3_OUT_CHANNELS 256
#define CNN_CONV3_KERNEL_SIZE 3
#define CNN_CONV3_PADDING 1

#define CNN_FC0_IN_SIZE 256
#define CNN_FC0_OUT_SIZE 128

#define CNN_FC1_IN_SIZE 128
#define CNN_FC1_OUT_SIZE 2

#define CNN_POOL0_KERNEL_SIZE 2
#define CNN_POOL0_STRIDE 2

#endif
