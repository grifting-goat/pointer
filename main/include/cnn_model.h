#ifndef cnn_model_H_
#define cnn_model_H_

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define GESTURE_N_CHANNELS  6
#define GESTURE_SEQ_LEN     108
#define GESTURE_INPUT_SIZE  (GESTURE_N_CHANNELS * GESTURE_SEQ_LEN)  // 648 floats
#define GESTURE_N_CLASSES   2   // change to your n_gestures

typedef struct {
    int class_id;
    float confidence;
    float motion_score;
    bool triggered;
} cnn_result_t;

// Opaque handle — internals are C++ and hidden from C callers
typedef struct cnn_model_t cnn_model_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load model from flash partition labeled "model".
 * Returns allocated handle on success, NULL on failure.
 */
cnn_model_t *cnn_model_init();

/**
 * Run inference on a float buffer laid out as [108][6] row-major
 * (time-major, then channel), matching the espdl model input shape.
 * Returns a fully-populated result including trigger state.
 */
esp_err_t cnn_model_infer(cnn_model_t *handle,
                               const float input[GESTURE_INPUT_SIZE],
                               cnn_result_t *out_result);

/** Free all resources. */
void cnn_model_deinit(cnn_model_t *handle);

#ifdef __cplusplus
}
#endif

#endif // cnn_model_H_