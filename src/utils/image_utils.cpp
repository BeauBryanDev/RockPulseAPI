#include "utils/image_utils.hpp"
#include <opencv2/objdetect/aruco_detector.hpp>
#include <iostream>
#include <cmath>

// ArUco marker detection for px->cm calibration


namespace  RockPulse {

namespace ImageUtils {


// -------------------------------------------------------------
CalibrationResult detectCalibrationMarker(
    const cv::Mat& image,
    float          marker_real_cm,
    int            dictionary_id

)    {
    CalibrationResult result;

    if (image.empty()) {
        result.error_msg = "Empty image passed to calibration.";
        return result;
    }

    cv::Mat gray;

    if (image.channels() == 3) {

        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    } else {

        gray = image.clone();
    }

    // 2. Build ArUco detector
    // ArucoDetector is the modern API (OpenCV 4.7+).

    cv::aruco::Dictionary dictionary =
        cv::aruco::getPredefinedDictionary(dictionary_id);

    cv::aruco::DetectorParameters params =
        cv::aruco::DetectorParameters();

    // Improve detection under uneven belt lighting
    params.adaptiveThreshWinSizeMin  = 3;
    params.adaptiveThreshWinSizeMax  = 53;
    params.adaptiveThreshWinSizeStep = 10;
    params.minMarkerPerimeterRate    = 0.02f;
    params.maxMarkerPerimeterRate    = 0.5f;
    params.polygonalApproxAccuracyRate = 0.05f;

    cv::aruco::ArucoDetector detector(dictionary, params);

    std::vector<std::vector<cv::Point2f>> corners;
    std::vector<int>                      ids;
    std::vector<std::vector<cv::Point2f>> rejected;

    detector.detectMarkers(gray, corners, ids, rejected);

    if (ids.empty()) {

        result.success   = false;
        result.error_msg =
            "No ArUco marker detected in image. "
            "Ensure a printed DICT_4X4_50 marker is visible "
            "and not hidden behind by rocks.";

        return result;
    }

    // If multiple markers detected, It use the largest one.
    // Largest = most likely to be the calibration marker
    // and not a spurious detection.

    int    best_idx      = 0;
    float  best_side_px  = 0.0f;

    for (int i = 0; i < static_cast<int>(corners.size()); ++i) {
        // Compute mean side length from the 4 corners
        // corners[i] has 4 points: TL, TR, BR, BL
        const auto& c = corners[i];

        float side0 = static_cast<float>(cv::norm(c[0] - c[1]));
        float side1 = static_cast<float>(cv::norm(c[1] - c[2]));
        float side2 = static_cast<float>(cv::norm(c[2] - c[3]));
        float side3 = static_cast<float>(cv::norm(c[3] - c[0]));

        float mean_side = (side0 + side1 + side2 + side3) / 4.0f;

        if (mean_side > best_side_px) {

            best_side_px = mean_side;
            best_idx     = i;
        }
    }

    // 5. Compute k from the best marker
    // k = marker_real_cm / marker_px

    if (best_side_px < 5.0f) {

        result.success   = false;
        result.error_msg =
            "Detected marker is too small in the image "
            "(" + std::to_string(static_cast<int>(best_side_px)) +
            " px). Move camera closer or use a larger marker.";

        return result;
    }

    result.success   = true;
    result.k         = marker_real_cm / best_side_px;
    result.marker_px = best_side_px;
    result.marker_id = ids[best_idx];

    std::cout << "[ImageUtils] ArUco marker ID "
            << result.marker_id
            << " detected. Side = "
            << result.marker_px << " px. "
            << "k = " << result.k << " cm/px." << std::endl;

    return result;
}

cv::Mat decodeImageBytes(const std::string& bytes)
{
    if (bytes.empty()) return cv::Mat();

    std::vector<uchar> buf(bytes.begin(), bytes.end());
    
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}


std::string validateImage(const cv::Mat& image)
{
    if (image.empty()) {

        return "Image could not be decoded. "
               "Supported formats: JPEG, PNG, BMP.";
    }

    if (image.channels() != 3) {

        return "Image must be a 3-channel BGR image.";
    }

    if (image.cols < 320 || image.rows < 320) {

        return "Image too small. Minimum resolution: 320x320.";
    }

    if (image.cols > 7680 || image.rows > 7680) {

        return "Image too large. Maximum resolution: 7680x7680.";
    }

    return "";
}

// end namespaces

}  // end of image utils

} // end of RockPulse 