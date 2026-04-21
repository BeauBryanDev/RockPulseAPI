#include "utils/image_utils.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

// ArUco marker detection for px->cm calibration

namespace  RockPulse {

namespace ImageUtils {


CalibrationResult detectCalibrationMarker(
    const cv::Mat& image,
    float   marker_real_cm,
    int     dictionary_id
    

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

    cv::Ptr<cv::aruco::Dictionary> dictionary =
        cv::aruco::getPredefinedDictionary(dictionary_id);

    cv::Ptr<cv::aruco::DetectorParameters> params =
        cv::aruco::DetectorParameters::create();

    // Improve detection under uneven belt lighting
    params->adaptiveThreshWinSizeMin    = 3;
    params->adaptiveThreshWinSizeMax    = 53;
    params->adaptiveThreshWinSizeStep   = 10;
    params->minMarkerPerimeterRate      = 0.02f;
    params->maxMarkerPerimeterRate      = 0.5f;
    params->polygonalApproxAccuracyRate = 0.05f;

    std::vector<std::vector<cv::Point2f>> corners;
    std::vector<int>                      ids;
    std::vector<std::vector<cv::Point2f>> rejected;

    cv::aruco::detectMarkers(gray, dictionary, corners, ids, params, rejected);

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

    //  Compute k from the best marker
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
    result.corners =  corners[best_idx];  // TL, TR, BR, BL

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

void drawCalibrationMarker(
    cv::Mat&                        image,
    const CalibrationResult&        calib_result,
    const std::vector<cv::Point2f>& marker_corners

)  {

    if (!calib_result.success || marker_corners.size() != 4) {
        return;
    }

    // Convert Point2f corners to Point for drawing
    std::vector<cv::Point> pts;
    pts.reserve(4);

    for (const auto& p : marker_corners) {

        pts.emplace_back(
            static_cast<int>(p.x),
            static_cast<int>(p.y)
        );
    }

    // Draw the four sides of the marker as a thick red polygon
    const cv::Scalar RED(0, 0, 255);
    const int        THICKNESS = 3;

    for (int i = 0; i < 4; ++i) {

        cv::line(image, pts[i], pts[(i + 1) % 4], RED, THICKNESS);
    }

    // Draw a small circle at each corner for precision check
    for (const auto& pt : pts) {

        cv::circle(image, pt, 5, RED, -1);
    }

    // Label above the marker: ID and k value
    std::ostringstream label;
    label << "ArUco ID:"  << calib_result.marker_id
          << "  k="       << std::fixed
          << std::setprecision(4) << calib_result.k
          << " cm/px";

    int baseline = 0;
    cv::Size text_size = cv::getTextSize(
        label.str(),
        cv::FONT_HERSHEY_SIMPLEX,
        0.55, 2, &baseline
    );

    // Background rectangle for label readability
    cv::Point label_tl(pts[0].x, pts[0].y - text_size.height - 10);
    cv::Point label_br(pts[0].x + text_size.width + 6,
                       pts[0].y - 4);

    // Clamp to image bounds
    label_tl.x = std::max(0, label_tl.x);
    label_tl.y = std::max(0, label_tl.y);
    label_br.x = std::min(image.cols, label_br.x);
    label_br.y = std::min(image.rows, label_br.y);

    cv::rectangle(image, label_tl, label_br,
                  cv::Scalar(0, 0, 0), cv::FILLED);

    cv::putText(
        image,
        label.str(),
        cv::Point(label_tl.x + 3, pts[0].y - 6),
        cv::FONT_HERSHEY_SIMPLEX,
        0.55, RED, 2
    );
}

// end namespaces

}  // end of image utils

} // end of RockPulse 