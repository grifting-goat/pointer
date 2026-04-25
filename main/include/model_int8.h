#ifndef MODEL_INT8_H
#define MODEL_INT8_H

#include <stdint.h>

extern const int net_0_weight_rows;
extern const int net_0_weight_cols;
extern const int8_t net_0_weight_int8[331776];
extern const float net_0_weight_scale[512];

extern const int net_0_bias_len;
extern const float net_0_bias[512];

extern const int net_3_weight_rows;
extern const int net_3_weight_cols;
extern const int8_t net_3_weight_int8[131072];
extern const float net_3_weight_scale[256];

extern const int net_3_bias_len;
extern const float net_3_bias[256];

extern const int net_5_weight_rows;
extern const int net_5_weight_cols;
extern const int8_t net_5_weight_int8[512];
extern const float net_5_weight_scale[2];

extern const int net_5_bias_len;
extern const float net_5_bias[2];

extern const int input_scaler_len;
extern const float input_scaler_mean[648];
extern const float input_scaler_std[648];

#endif
