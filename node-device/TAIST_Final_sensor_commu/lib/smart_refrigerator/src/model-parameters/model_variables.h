/* Generated by Edge Impulse
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _EI_CLASSIFIER_MODEL_VARIABLES_H_
#define _EI_CLASSIFIER_MODEL_VARIABLES_H_

#include <stdint.h>
#include "model_metadata.h"

#include "tflite-model/tflite_learn_5_compiled.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/engines.h"

const char* ei_classifier_inferencing_categories[] = { "m150", "nescafe", "splash" };

uint8_t ei_dsp_config_4_axes[] = { 0 };
const uint32_t ei_dsp_config_4_axes_size = 1;
ei_dsp_config_image_t ei_dsp_config_4 = {
    4, // uint32_t blockId
    1, // int implementationVersion
    1, // int length of axes
    "RGB" // select channels
};

const size_t ei_dsp_blocks_size = 1;
ei_model_dsp_t ei_dsp_blocks[ei_dsp_blocks_size] = {
    { // DSP block 4
        4,
        49152, // output size
        &extract_image_features, // DSP function pointer
        (void*)&ei_dsp_config_4, // pointer to config struct
        ei_dsp_config_4_axes, // array of offsets into the input stream, one for each axis
        ei_dsp_config_4_axes_size, // number of axes
        1, // version
        nullptr, // factory function
    }
};
const ei_config_tflite_eon_graph_t ei_config_tflite_graph_5 = {
    .implementation_version = 1,
    .model_init = &tflite_learn_5_init,
    .model_invoke = &tflite_learn_5_invoke,
    .model_reset = &tflite_learn_5_reset,
    .model_input = &tflite_learn_5_input,
    .model_output = &tflite_learn_5_output,
};

const ei_learning_block_config_tflite_graph_t ei_learning_block_config_5 = {
    .implementation_version = 1,
    .classification_mode = EI_CLASSIFIER_CLASSIFICATION_MODE_OBJECT_DETECTION,
    .block_id = 5,
    .object_detection = 1,
    .object_detection_last_layer = EI_CLASSIFIER_LAST_LAYER_FOMO,
    .output_data_tensor = 0,
    .output_labels_tensor = 1,
    .output_score_tensor = 2,
    .threshold = 0.5,
    .quantized = 1,
    .compiled = 1,
    .graph_config = (void*)&ei_config_tflite_graph_5
};

const size_t ei_learning_blocks_size = 1;
const uint32_t ei_learning_block_5_inputs[1] = { 4 };
const uint32_t ei_learning_block_5_inputs_size = 1;
const ei_learning_block_t ei_learning_blocks[ei_learning_blocks_size] = {
    {
        5,
        false,
        &run_nn_inference,
        (void*)&ei_learning_block_config_5,
        EI_CLASSIFIER_IMAGE_SCALING_NONE,
        ei_learning_block_5_inputs,
        ei_learning_block_5_inputs_size,
        1024
    },
};

const ei_model_performance_calibration_t ei_calibration = {
    1, /* integer version number */
    false, /* has configured performance calibration */
    (int32_t)(EI_CLASSIFIER_RAW_SAMPLE_COUNT / ((EI_CLASSIFIER_FREQUENCY > 0) ? EI_CLASSIFIER_FREQUENCY : 1)) * 1000, /* Model window */
    0.8f, /* Default threshold */
    (int32_t)(EI_CLASSIFIER_RAW_SAMPLE_COUNT / ((EI_CLASSIFIER_FREQUENCY > 0) ? EI_CLASSIFIER_FREQUENCY : 1)) * 500, /* Half of model window */
    0   /* Don't use flags */
};
const ei_object_detection_nms_config_t ei_object_detection_nms = {
    0.0f, /* NMS confidence threshold */
    0.2f  /* NMS IOU threshold */
};

const ei_impulse_t impulse_376729_0 = {
    .project_id = 376729,
    .project_owner = "Lazy_person",
    .project_name = "refrigerator",
    .deploy_version = 2,

    .nn_input_frame_size = 49152,
    .raw_sample_count = 16384,
    .raw_samples_per_frame = 1,
    .dsp_input_frame_size = 16384 * 1,
    .input_width = 128,
    .input_height = 128,
    .input_frames = 1,
    .interval_ms = 1,
    .frequency = 0,
    .dsp_blocks_size = ei_dsp_blocks_size,
    .dsp_blocks = ei_dsp_blocks,
    
    .object_detection_count = 10,
    .fomo_output_size = 16,
    
    .tflite_output_features_count = 1024,
    .learning_blocks_size = ei_learning_blocks_size,
    .learning_blocks = ei_learning_blocks,

    .inferencing_engine = EI_CLASSIFIER_TFLITE,

    .sensor = EI_CLASSIFIER_SENSOR_CAMERA,
    .fusion_string = "image",
    .slice_size = (16384/4),
    .slices_per_model_window = 4,

    .has_anomaly = EI_ANOMALY_TYPE_UNKNOWN,
    .label_count = 3,
    .calibration = ei_calibration,
    .categories = ei_classifier_inferencing_categories,
    .object_detection_nms = ei_object_detection_nms
};

ei_impulse_handle_t impulse_handle_376729_0 = ei_impulse_handle_t( &impulse_376729_0 );
ei_impulse_handle_t& ei_default_impulse = impulse_handle_376729_0;

#endif // _EI_CLASSIFIER_MODEL_METADATA_H_
