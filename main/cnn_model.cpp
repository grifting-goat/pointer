#include "cnn_model.h"
#include "dl_model_base.hpp"   // dl::Model, dl::TensorBase, fbs::MODEL_LOCATION_IN_FLASH_PARTITION
#include "esp_log.h"
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>

static const char *TAG = "cnn_model";
static const char *MODEL_PARTITION_LABEL = "model";
static const float CIRCLE_CONF_THRESHOLD = 0.75f;
static const float CIRCLE_MOTION_THRESHOLD = 0.0f;

static int tensor_shape_product(const std::vector<int> &shape)
{
    if (shape.empty()) {
        return 0;
    }
    int prod = 1;
    for (int d : shape) {
        if (d <= 0) {
            return 0;
        }
        prod *= d;
    }
    return prod;
}

struct cnn_model_t {
    dl::Model *model;
    int input_elements;
    int output_elements;
};


extern "C" cnn_model_t *cnn_model_init()
{
    ESP_LOGI(TAG, "Loading model from partition '%s'", MODEL_PARTITION_LABEL);

    dl::Model *m = new dl::Model(
        MODEL_PARTITION_LABEL,
        fbs::MODEL_LOCATION_IN_FLASH_PARTITION
    );

    if (!m) {
        ESP_LOGE(TAG, "Failed to instantiate dl::Model");
        return nullptr;
    }

    std::map<std::string, dl::TensorBase *> inputs = m->get_inputs();
    std::map<std::string, dl::TensorBase *> outputs = m->get_outputs();
    if (inputs.empty() || outputs.empty()) {
        ESP_LOGE(TAG, "Model load failed from partition '%s'", MODEL_PARTITION_LABEL);
        delete m;
        return nullptr;
    }

    dl::TensorBase *model_input = inputs.begin()->second;
    dl::TensorBase *model_output = outputs.begin()->second;
    const int input_elements = tensor_shape_product(model_input->shape);
    const int output_elements = tensor_shape_product(model_output->shape);

    ESP_LOGI(TAG,
             "Model IO shapes: input=%s (%d), output=%s (%d)",
             dl::vector_to_string(model_input->shape).c_str(),
             input_elements,
             dl::vector_to_string(model_output->shape).c_str(),
             output_elements);

    if (input_elements != GESTURE_INPUT_SIZE || output_elements <= 0) {
        ESP_LOGE(TAG,
                 "Unexpected model IO size. expected input=%d got input=%d output=%d",
                 GESTURE_INPUT_SIZE,
                 input_elements,
                 output_elements);
        delete m;
        return nullptr;
    }

    if (m->test() != ESP_OK) {
        ESP_LOGW(TAG, "Model self-test failed — check exponent / quantization");
    }

    cnn_model_t *handle = new cnn_model_t{m, input_elements, output_elements};
    ESP_LOGI(TAG, "Model loaded OK");
    return handle;
}

extern "C" esp_err_t cnn_model_infer(cnn_model_t *handle, const float input[GESTURE_INPUT_SIZE], cnn_result_t *out_result)
{
    if (!handle || !handle->model || !input || !out_result) {
        return ESP_ERR_INVALID_ARG;
    }

    dl::Model *model = handle->model;

    // --- 1. Get the model's pre-allocated input tensor
    std::map<std::string, dl::TensorBase *> inputs = model->get_inputs();
    if (inputs.empty()) {
        ESP_LOGE(TAG, "Model inputs map is empty");
        return ESP_FAIL;
    }
    dl::TensorBase *model_input = inputs.begin()->second;

    dl::TensorBase float_input(model_input->shape, (void *)input, 0, dl::DATA_TYPE_FLOAT, false);
    model_input->assign(&float_input);

    model->run(dl::RUNTIME_MODE_AUTO);

    std::map<std::string, dl::TensorBase *> outputs = model->get_outputs();
    if (outputs.empty()) {
        ESP_LOGE(TAG, "Model outputs map is empty");
        return ESP_FAIL;
    }
    dl::TensorBase *model_output = outputs.begin()->second;

    const int output_count = tensor_shape_product(model_output->shape);
    if (output_count <= 0) {
        ESP_LOGE(TAG, "Invalid output tensor shape");
        return ESP_FAIL;
    }
    dl::TensorBase float_output(model_output->shape, nullptr, 0, dl::DATA_TYPE_FLOAT);
    float_output.assign(model_output);   // dequantizes int8 → float

    float *scores = (float *)float_output.get_element_ptr();
    
    float max_logit = scores[0];
    for (int i = 1; i < output_count; i++) {
        if (scores[i] > max_logit) {
            max_logit = scores[i];
        }
    }

    float probs[GESTURE_N_CLASSES] = {0};
    float sum_exp = 0.0f;
    const int n_prob = std::min(output_count, GESTURE_N_CLASSES);
    for (int i = 0; i < n_prob; i++) {
        probs[i] = std::exp(scores[i] - max_logit);
        sum_exp += probs[i];
    }
    if (sum_exp > 0.0f) {
        for (int i = 0; i < n_prob; i++) {
            probs[i] /= sum_exp;
        }
    }

    int best = 0;
    float best_val = -FLT_MAX;
    for (int i = 0; i < n_prob; i++) {
        if (probs[i] > best_val) {
            best_val = probs[i];
            best = i;
        }
    }

    out_result->class_id = best;
    out_result->confidence = best_val;
    out_result->motion_score = 1.0f;
    out_result->triggered =
        (out_result->class_id == 1) &&
        (out_result->confidence >= CIRCLE_CONF_THRESHOLD) &&
        (out_result->motion_score >= CIRCLE_MOTION_THRESHOLD);

    ESP_LOGD(TAG, "class=%d score=%.3f", best, best_val);
    return ESP_OK;
} 

extern "C" void cnn_model_deinit(cnn_model_t *handle)
{
    if (handle) {
        delete handle->model;
        delete handle;
    }
}