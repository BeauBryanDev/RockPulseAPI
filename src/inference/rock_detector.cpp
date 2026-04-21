#include "inference/rock_detector.hpp"
#include <opencv2/dnn.hpp>
#include <stdexcept>
#include <iostream>
#include <filesystem>


namespace RockPulse {

    // Loads ONNX model, resolves I/O names, caches them.
    // Called ONCE at server startup.

RockDetector::RockDetector(  const DetectorConfig& config ) 
: config_( config )
, ready_( false )
, env_( ORT_LOGGING_LEVEL_WARNING , "RockPulseAPI" )

{
    try {
        // validate model path bedore load
        if ( !std::filesystem::exists( config_.model_path ) ) {
            throw std::runtime_error(
                "Model Not Found: " + config_.model_path
            );
        }

        // Extract model filename for /api/health response
        model_name_ = std::filesystem::path(
            config_.model_path
        ).filename().string();

        // Configure session options
        session_opts_.SetIntraOpNumThreads(config_.intra_threads);
        session_opts_.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL
        );

        // Load ONNX model
        session_ = std::make_unique<Ort::Session>(
            env_,
            config_.model_path.c_str(),
            session_opts_
        );

        // Resolve and cache I/O names
        // These are heap-allocated by ONNX - copy to std::string
        input_name_   = session_->GetInputNameAllocated(
            0, allocator_).get();
        output0_name_ = session_->GetOutputNameAllocated(
            0, allocator_).get();
        output1_name_ = session_->GetOutputNameAllocated(
            1, allocator_).get();

        ready_ = true;

        std::cout << "[RockDetector] Model loaded: "
                  << model_name_ << std::endl;
        std::cout << "[RockDetector] Input  : " << input_name_   << std::endl;
        std::cout << "[RockDetector] Output0: " << output0_name_ << std::endl;
        std::cout << "[RockDetector] Output1: " << output1_name_ << std::endl;

    } catch (const Ort::Exception& e) {
        std::cerr << "[RockDetector] ONNX error: " << e.what() << std::endl;
        ready_ = false;
    } catch (const std::exception& e) {
        std::cerr << "[RockDetector] Init error: " << e.what() << std::endl;
        ready_ = false;
    }
     
}

bool RockDetector::isReady() const {
    return ready_;
}

const std::string& RockDetector::modelName() const {
    return model_name_;
}

// Resize -> normalize [0,1] -> HWC to NCHW via cv::split

std::vector<float> RockDetector::preprocess(
    const cv::Mat& image,
    float&         out_scale_x,
    float&         out_scale_y) const
{
    out_scale_x = static_cast<float>(image.cols) / config_.input_w;
    out_scale_y = static_cast<float>(image.rows) / config_.input_h;

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(config_.input_w, config_.input_h));

    // Convert to float and normalize to [0, 1]
    cv::Mat float_img;
    resized.convertTo(float_img, CV_32F, 1.0 / 255.0);

    // Split BGR channels into separate planes
    std::vector<cv::Mat> channels(3);
    cv::split(float_img, channels);

    // Pack into NCHW flat vector: [C0_row0...C0_rowN, C1..., C2...]
    const int plane_size = config_.input_h * config_.input_w;
    std::vector<float> nchw(3 * plane_size);

    for (int c = 0; c < 3; ++c) {
        // channels[c] is continuous after cv::split
        std::memcpy(
            nchw.data() + c * plane_size,
            channels[c].ptr<float>(),
            plane_size * sizeof(float)
        );
    }

    return nchw;
}
// Parses output0 tensor shape (1, 37, 8400)
// Layout: [cx, cy, w, h, conf, mask_coeff_0..31] x 8400
void RockDetector::parseDetections(
    const float*                     data0,
    int                              num_preds,
    float                            scale_x,
    float                            scale_y,
    int                              img_w,
    int                              img_h,
    std::vector<cv::Rect>&           out_boxes,
    std::vector<float>&              out_scores,
    std::vector<std::vector<float>>& out_mask_coeffs) const
{
    for (int i = 0; i < num_preds; ++i) {

        // Confidence score is at row index 4
        float conf = data0[4 * num_preds + i];
        if (conf < config_.conf_threshold) continue;

        // Box center and dimensions in model input space (640x640)
        float cx = data0[0 * num_preds + i];
        float cy = data0[1 * num_preds + i];
        float w  = data0[2 * num_preds + i];
        float h  = data0[3 * num_preds + i];

        // Scale back to original image coordinates
        int x1 = static_cast<int>((cx - w / 2.0f) * scale_x);
        int y1 = static_cast<int>((cy - h / 2.0f) * scale_y);
        int bw = static_cast<int>(w * scale_x);
        int bh = static_cast<int>(h * scale_y);

        // Clamp to image boundaries
        x1 = std::max(0, std::min(x1, img_w - 1));
        y1 = std::max(0, std::min(y1, img_h - 1));
        bw = std::max(1, std::min(bw, img_w - x1));
        bh = std::max(1, std::min(bh, img_h - y1));

        out_boxes.emplace_back(x1, y1, bw, bh);
        out_scores.push_back(conf);

        // Extract 32 mask coefficients starting at row index 5
        std::vector<float> coeffs(config_.num_masks);
        for (int c = 0; c < config_.num_masks; ++c) {
            coeffs[c] = data0[(5 + c) * num_preds + i];
        }
        out_mask_coeffs.push_back(std::move(coeffs));
    }
}

// Reconstructs binary mask from coefficients and proto map.
// mask = sigmoid(coeffs @ proto_map), resized, thresholded
cv::Mat RockDetector::assembleMask(
    const std::vector<float>& mask_coeffs,
    const cv::Mat&            proto_map,
    const cv::Rect&           bbox,
    int                       orig_w,
    int                       orig_h) const
{
    // coeffs: (1, 32) x proto_map: (32, 160*160) = (1, 160*160)
    cv::Mat coeff_mat(
        1,
        config_.num_masks,
        CV_32F,
        const_cast<float*>(mask_coeffs.data())
    );

    cv::Mat mask_flat = coeff_mat * proto_map;
    cv::Mat mask_160  = mask_flat.reshape(1, config_.mask_h);

    // Apply sigmoid activation
    cv::exp(-mask_160, mask_160);
    mask_160 = 1.0f / (1.0f + mask_160);

    // Resize to original image dimensions
    cv::Mat mask_full;
    cv::resize(mask_160, mask_full, cv::Size(orig_w, orig_h));

    // Crop to bounding box ROI and threshold to binary
    cv::Mat mask_roi = mask_full(bbox);
    cv::Mat bin_mask;
    cv::threshold(mask_roi, bin_mask, 0.5, 255.0, cv::THRESH_BINARY);
    bin_mask.convertTo(bin_mask, CV_8U);

    return bin_mask;
}

// Draws bounding boxes, contours and metric labels
cv::Mat RockDetector::annotate(
    const cv::Mat&                  image,
    const std::vector<RockMetrics>& rocks) const
{
    cv::Mat annotated = image.clone();

    for (const auto& r : rocks) {
        cv::Rect bbox(r.bbox_x, r.bbox_y, r.bbox_w, r.bbox_h);

        // Bounding box
        cv::rectangle(annotated, bbox, cv::Scalar(0, 255, 0), 2);

        // Label: rock id, volume, confidence
        std::string label =
            "#"  + std::to_string(r.rock_id)       +
            " V:"+ std::to_string(
                       static_cast<int>(r.volume_cm3)) + "cm3"  +
            " c:"+ std::to_string(
                       static_cast<int>(r.confidence * 100))     + "%";

        int baseline  = 0;
        cv::Size text_size = cv::getTextSize(
            label, cv::FONT_HERSHEY_SIMPLEX, 0.38, 1, &baseline
        );

        cv::Point label_origin(
            r.bbox_x,
            std::max(r.bbox_y - 5, text_size.height + 2)
        );

        // Background rectangle for readability
        cv::rectangle(
            annotated,
            cv::Point(label_origin.x, label_origin.y - text_size.height - 2),
            cv::Point(label_origin.x + text_size.width, label_origin.y + baseline),
            cv::Scalar(0, 0, 0),
            cv::FILLED
        );

        cv::putText(
            annotated, label, label_origin,
            cv::FONT_HERSHEY_SIMPLEX, 0.38,
            cv::Scalar(255, 255, 255), 1
        );
    }

    // Rock count overlay top-left
    std::string count_label = "Rocks: " + std::to_string(rocks.size());
    cv::putText(
        annotated, count_label,
        cv::Point(10, 28),
        cv::FONT_HERSHEY_SIMPLEX, 0.8,
        cv::Scalar(0, 255, 255), 2
    );

    return annotated;
}

// Full pipeline: preprocess -> inference -> NMS ->
// mask assembly -> metrics -> annotate
DetectionResult RockDetector::detect(const DetectionJob& job) const
{
    DetectionResult result;
    result.job_id = job.job_id;

    if (!ready_) {
        result.success   = false;
        result.error_msg = "Detector not ready - model failed to load.";
        return result;
    }

    if (job.image.empty()) {
        result.success   = false;
        result.error_msg = "Empty image received.";
        return result;
    }

    try {
        const int orig_w = job.image.cols;
        const int orig_h = job.image.rows;

        // pre-Process

        float scale_x = 0.0f;
        float scale_y = 0.0f;
        std::vector<float> input_data = preprocess(
            job.image, scale_x, scale_y
        );

        // Build up input Tensor 
        std::array<int64_t, 4> input_shape = {
            1, 3,
            static_cast<int64_t>(config_.input_h),
            static_cast<int64_t>(config_.input_w)
        };

        Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault
        );

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            mem_info,
            input_data.data(),
            input_data.size(),
            input_shape.data(),
            input_shape.size()
        );

        // Run Inference 
        const char* input_names[]  = { input_name_.c_str()   };
        const char* output_names[] = {
            output0_name_.c_str(),
            output1_name_.c_str()
        };

        auto outputs = session_->Run(
            Ort::RunOptions{ nullptr },
            input_names,  &input_tensor, 1,
            output_names, 2
        );

        // Extract Raw Points 
        float* data0     = outputs[0].GetTensorMutableData<float>();
        float* data1     = outputs[1].GetTensorMutableData<float>();
        auto   shape0    = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int    num_preds = static_cast<int>(shape0[2]);  // 8400

        // Build proto map once for all mask assemblies
        // Shape: (32, 160*160)
        cv::Mat proto_map(
            config_.num_masks,
            config_.mask_h * config_.mask_w,
            CV_32F,
            data1
        );

        // Parse Detections aboive CONF_THRESHOLD 
        std::vector<cv::Rect>             boxes;
        std::vector<float>                scores;
        std::vector<std::vector<float>>   mask_coeffs;

        parseDetections(
            data0, num_preds,
            scale_x, scale_y,
            orig_w, orig_h,
            boxes, scores, mask_coeffs
        );

        // NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(
            boxes, scores,
            config_.conf_threshold,
            config_.nms_threshold,
            indices
        );

        // Mask Assembly  + Detections Metrics
        int rock_id = 0;
        result.rocks.reserve(indices.size());

        for (int idx : indices) {
            ++rock_id;

            cv::Mat bin_mask = assembleMask(
                mask_coeffs[idx],
                proto_map,
                boxes[idx],
                orig_w, orig_h
            );

            if (bin_mask.empty()) continue;

            RockMetrics metrics = MetricsCalculator::compute(
                bin_mask,
                boxes[idx],
                scores[idx],
                rock_id,
                job.k,
                job.density,
                job.fi_params
            );

            result.rocks.push_back(std::move(metrics));
        }

        result.rock_count  = static_cast<int>(result.rocks.size());
        result.output_image = annotate(job.image, result.rocks);
        result.success      = true;

    } catch (const Ort::Exception& e) {
        result.success   = false;
        result.error_msg = std::string("ONNX inference error: ") + e.what();
        std::cerr << "[RockDetector] " << result.error_msg << std::endl;

    } catch (const cv::Exception& e) {
        result.success   = false;
        result.error_msg = std::string("OpenCV error: ") + e.what();
        std::cerr << "[RockDetector] " << result.error_msg << std::endl;

    } catch (const std::exception& e) {
        result.success   = false;
        result.error_msg = std::string("Unexpected error: ") + e.what();
        std::cerr << "[RockDetector] " << result.error_msg << std::endl;
    }

    return result;
}

// End of Rock namespace 

}