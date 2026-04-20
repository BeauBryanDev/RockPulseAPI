#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <optional>
#include <string>

//  ArUco marker detection for px->cm calibration
// and image preprocessing utilities


namespace RockPulse {

namespace ImageUtils {

// On success: k is the calibration factor cm/px.
// On failure: k is 0.0 and error_msg is set.
struct CalibrationResult {
    bool        success       = false;
    float       k             = 0.0f;   // cm/px
    float       marker_px     = 0.0f;   // detected marker side in px
    int         marker_id     = -1;     // ArUco marker ID found
    std::string error_msg;
};
// Detects a single ArUco marker in the image and computes
// the calibration factor k = marker_real_cm / marker_px.

CalibrationResult detectCalibrationMarker(
    const cv::Mat& image,    // BGR image as received from the API
    float    marker_real_cm,  // physical marker side length in cm 
    int      dictionary_id = cv::aruco::DICT_4X4_50  // ArUco dictionary (default: DICT_4X4_50)
);                // Returns CalibrationResult. If no marker is found,

// Decodes raw bytes from multipart upload to cv::Mat.
// Returns empty Mat on failure.
cv::Mat decodeImageBytes(const std::string& bytes);

// Checks image is not empty, has 3 channels, and dimensions
// are within acceptable bounds for inference.

std::string validateImage(const cv::Mat& image);

}  // namespace ImageUtils

}  // end of namespace RockPulse 
