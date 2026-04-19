#pragma once

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

#include "inference/metrics_calculator.hpp"

// ONNX session lifecycle, image preprocessing,
// YOLOv8-seg inference, mask assembly, NMS
// Delivers: vector<RockMetrics> to the Crow handler

namespace  RockPulse {
    
    // Loaded once at StartUp  from config/app_settings.json
    // It paased into RockDetector  constructos 
    struct DetectorConfig {
    std::string model_path     = "models/yolo_v8_nano_seg.onnx";
    int   input_w      = 640;
    int    input_h        = 640;
    float   conf_threshold = 0.80f;
    float nms_threshold  = 0.20f;
    int   num_classes    = 1;
    int num_masks      = 32;
    int   mask_h      = 160;
    int    mask_w      = 160;
    int   intra_threads  = 1;
};

// Input context for a single inference request.
// Populated by the Crow handler from DB lookups and
// the incoming multipart/form-data request
struct DetectionJob {
    std::string job_id;
    cv::Mat     image;            // decoded image, BGR, original resolution
    float       k;                // calibration factor cm/px from ArUco
    float       density;          // conveyor belt base_rock_density (kg/m3)
    FIParams    fi_params;        // alpha, beta, gamma from locations table
};
// Output of RockDetector::detect().
// Returned to the Crow handler for JSON serialization
// and PostgreSQL insertion.
struct DetectionResult {
    std::string            job_id;
    int                    rock_count    = 0;
    std::vector<RockMetrics> rocks;       // one entry per detected rock
    cv::Mat                output_image; // annotated image for saving
    bool                   success       = false;
    std::string            error_msg;
};

class RockDetector {
public:

    explicit RockDetector(const DetectorConfig& config);

    // Non-copyable - ONNX session owns native resources
    RockDetector(const RockDetector&)            = delete;
    RockDetector& operator=(const RockDetector&) = delete;

    // Movable
    RockDetector(RockDetector&&)                 = default;
    RockDetector& operator=(RockDetector&&)      = default;

    ~RockDetector() = default;
    // Full pipeline: preprocess -> inference -> postprocess
    // -> mask assembly -> metrics -> annotate
    // Thread-safe. Can be called concurrently from Crow.
    DetectionResult detect(const DetectionJob& job) const;
    // Returns true if ONNX session loaded successfully.
    // Used by GET /api/health handler.
    bool isReady() const;
    // modelName()
    // Returns the model filename for /api/health response.
    const std::string& modelName() const;

private:

    // Load the ONNX model and create the session
    // Resize -> normalize -> preprocess [ 0,1 ) >>> HWC to NCHW layout 
    std::vector<float> preprocess(
    const cv::Mat& image,
    float&  out_scale_x,
    float&  out_scale_y
    ) const;

    // Parses output0 tensor (1, 37, 8400).
    // Extracts boxes, confidence scores, and 32 mask coefficients
    // for candidates above conf_threshold
    void parseDetections(
        const float*  data0,
        int     num_preds,
        float  scale_x,
        float   scale_y,
        int     img_w,
        int    img_h,
        std::vector<cv::Rect>&    out_boxes,
        std::vector<float>&       out_scores,
        std::vector<std::vector<float>>& out_mask_coeffs
    ) const;
    // Re Make Bin Mask 
    cv::Mat assembleMask(
        const std::vector<float>& mask_coeffs,
        const cv::Mat&            proto_map,
        const cv::Rect&           bbox,
        int                       orig_w,
        int                       orig_h
    ) const;
    // Draws bounding boxes, mask overlays, and metric labels
    // on a copy of the original image.
    // Returns annotated BGR image for saving to disk.
    cv::Mat annotate(
        const cv::Mat&                 image,
        const std::vector<RockMetrics>& rocks
    ) const;

    DetectorConfig     config_;
    bool     ready_  = false;
    std::string   model_name_;

    // ONNX Runtime - owned by this instance
    Ort::Env       env_;
    Ort::SessionOptions  session_opts_;
    std::unique_ptr<Ort::Session> session_;
    Ort::AllocatorWithDefaultOptions allocator_;

    // Cached I/O names - resolved once at construction
    std::string     input_name_;
    std::string      output0_name_;
    std::string     output1_name_;
};

} // namespace RockPulse 


