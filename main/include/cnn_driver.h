#ifndef CNN_DRIVER_H
#define CNN_DRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "circ_buf.h"
#include "model_cnn.h"

typedef struct {
    int class_id;
    float confidence;
    float motion_score;
} cnn_result_t;

/**
 * Run CNN inference on a raw sensor window.
 */
void cnn_predict_raw(const int16_t *raw_window, size_t raw_len, cnn_result_t *out);

/**
 * Run CNN inference directly from a circular buffer snapshot.
 */
void cnn_predict_buffer(const Circ_buf *buf, cnn_result_t *out);

/**
 * make the workspace for inference
 */

bool alloc_cnn_inference_workspace(void);



#endif //CNN_DRIVER_H