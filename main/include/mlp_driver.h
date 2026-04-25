#ifndef MLP_DRIVER_H
#define MLP_DRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "circ_buf.h"
#include "model_int8.h"

#define MLP_INPUT_SIZE 648
#define MLP_HIDDEN0_SIZE 512
#define MLP_HIDDEN1_SIZE 256
#define MLP_OUTPUT_SIZE 2
#define MLP_CHANNELS 6

#define mlp_w0_int8 net_0_weight_int8
#define mlp_w0_scale net_0_weight_scale
#define mlp_b0 net_0_bias

#define mlp_w1_int8 net_3_weight_int8
#define mlp_w1_scale net_3_weight_scale
#define mlp_b1 net_3_bias

#define mlp_w2_int8 net_5_weight_int8
#define mlp_w2_scale net_5_weight_scale
#define mlp_b2 net_5_bias

typedef struct {
    int class_id;
    float confidence;
    float motion_score;
} mlp_result_t;

/**
 * Run MLP inference on a raw sensor window.
 */
void mlp_predict_raw(const int16_t *raw_window, size_t raw_len, mlp_result_t *out);

/**
 * Run MLP inference directly from a circular buffer snapshot.
 */
void mlp_predict_buffer(const Circ_buf *buf, mlp_result_t *out);

#endif /* MLP_DRIVER_H */ 
