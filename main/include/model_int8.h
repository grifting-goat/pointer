#ifndef MODEL_INT8_H
#define MODEL_INT8_H

#include <stdint.h>

extern const int net_0_weight_rows;
extern const int net_0_weight_cols;
extern const int8_t net_0_weight_int8[82944];
extern const float net_0_weight_scale[128];

extern const int net_0_bias_len;
extern const float net_0_bias[128];

extern const int net_3_weight_rows;
extern const int net_3_weight_cols;
extern const int8_t net_3_weight_int8[8192];
extern const float net_3_weight_scale[64];

extern const int net_3_bias_len;
extern const float net_3_bias[64];

extern const int net_5_weight_rows;
extern const int net_5_weight_cols;
extern const int8_t net_5_weight_int8[128];
extern const float net_5_weight_scale[2];

extern const int net_5_bias_len;
extern const float net_5_bias[2];

extern const int input_scaler_len;
extern const float input_scaler_mean[648];
extern const float input_scaler_std[648];

#endif
