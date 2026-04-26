#include "inference/metrics_calculator.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>


namespace RockPulse {

    // largestContour()
std::vector<std::vector<cv::Point>>::const_iterator  MetricsCalculator::largestContour(
    const std::vector<std::vector<cv::Point>>& contours)
{
    return std::max_element(
        contours.begin(),
        contours.end(),
        [](const std::vector<cv::Point>& a,
           const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        }
    );
}
// computeConvexMetrics()
void MetricsCalculator::computeConvexMetrics(
    const std::vector<cv::Point>& contour,
    int&    out_convex_area_px,
    double& out_convex_perimeter_px)
{
    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);

    // Convex hull area via fillPoly on a temporary mask
    double hull_area = cv::contourArea(hull);
    out_convex_area_px      = static_cast<int>(hull_area);
    out_convex_perimeter_px = cv::arcLength(hull, true);
}

// computeEllipseAxes()
bool MetricsCalculator::computeEllipseAxes(
    const std::vector<cv::Point>& contour,
    float   k,
    float&  out_major_cm,
    float&  out_minor_cm,
    float&  out_angle_deg)
{
    // cv::fitEllipse requires at least 5 points
    if (contour.size() < 5) {
        out_major_cm  = 0.0f;
        out_minor_cm  = 0.0f;
        out_angle_deg = 0.0f;
        return false;
    }

    cv::RotatedRect ellipse = cv::fitEllipse(contour);

    // fitEllipse returns size in pixels - convert to cm
    // size.width  = minor axis diameter
    // size.height = major axis diameter
    float major_px   = std::max(ellipse.size.width, ellipse.size.height);
    float minor_px   = std::min(ellipse.size.width, ellipse.size.height);

    out_major_cm  = major_px * k;
    out_minor_cm  = minor_px * k;
    out_angle_deg = ellipse.angle;

    return true;

}

// computeFragmentIndex()
float MetricsCalculator::computeFragmentIndex(
    double          solidity,
    double          convexity,
    float           aspect_ratio,
    const FIParams& fi_params)
{
    // Guard: default params mean not yet calibrated
    constexpr float UNCALIBRATED_SENTINEL = -1.0f;
    const bool is_default =
        (fi_params.alpha == 0.33f) &&
        (fi_params.beta  == 0.33f) &&
        (fi_params.gamma == 0.34f);

    if (is_default) {
        return UNCALIBRATED_SENTINEL;
    }

    float fi = static_cast<float>(
        fi_params.alpha * solidity  +
        fi_params.beta  * convexity +
        fi_params.gamma * aspect_ratio
    );

    // Clamp to [0.0, 1.0]
    return std::max(0.0f, std::min(1.0f, fi));
}
// compute() - main entry point
RockMetrics MetricsCalculator::compute(
    const cv::Mat&  bin_mask,
    const cv::Rect& bbox,
    float    confidence,
    int      rock_id,
    float    k,
    float    density,
    const FIParams& fi_params)
{
    RockMetrics m;

    m.rock_id    = rock_id;
    m.confidence = confidence;
    m.bbox_x     = bbox.x;
    m.bbox_y     = bbox.y;
    m.bbox_w     = bbox.width;
    m.bbox_h     = bbox.height;

// Extract Contours from binary Mask 

std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        bin_mask,
        contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_SIMPLE
    );

    if (contours.empty()) {
        return m;  // empty mask - return zeroed struct
    }

    auto it        = largestContour(contours);
    const auto& contour = *it;

    //Raw pixel measurements
    m.area_px      = static_cast<double>(cv::countNonZero(bin_mask));
    m.perimeter_px = cv::arcLength(contour, true);

    // OBB by minAreaRect - rotation invariant L and W

    cv::RotatedRect obb = cv::minAreaRect(contour);
    m.L_px = std::max(obb.size.width, obb.size.height);
    m.W_px = std::min(obb.size.width, obb.size.height);

    //  Convex hull metrics

    double convex_perimeter_px = 0.0;
    computeConvexMetrics(contour, m.convex_area_px, convex_perimeter_px);

    // Real-world conversions using calibration factor k
    m.area_cm2  = m.area_px    * static_cast<double>(k * k);
    m.perimeter_cm = m.perimeter_px * static_cast<double>(k);
    m.L_cm   = m.L_px * k;
    m.W_cm   = m.W_px * k;
    // H estimation - empirical ellipsoid assumption
    // H = (2/5) * (L_cm + W_cm)
     m.estimate_H_cm = 0.4f * (m.L_cm + m.W_cm);
    // Equivalent diameter: sqrt(4 * area_cm2 / pi)
    m.equiv_diameter = std::sqrt(
        4.0 * m.area_cm2 / CV_PI
    );

    // Ellipsoid: V = (pi/6) * L * W * H
    // Mass: density(kg/m3) * volume_cm3 * 0.001 -> grams
    m.volume_cm3 = static_cast<float>(
        (CV_PI / 6.0) *
        static_cast<double>(m.L_cm) *
        static_cast<double>(m.W_cm) *
        static_cast<double>(m.estimate_H_cm)
    );

    m.mass_g = density * m.volume_cm3 * 0.001f;

    // Shape Descriptors Metrics 

    // Aspet Ratio 
    m.aspect_ratio = (m.W_cm > 0.0f)
        ? m.L_cm / m.W_cm
        : 0.0f;

    // Sphericity - Cox Index: (4 * pi * area_px) / perimeter_px^2
    m.sphericity = (m.perimeter_px > 0.0)
        ? (4.0 * CV_PI * m.area_px) / (m.perimeter_px * m.perimeter_px)
        : 0.0;

    // Solidity: area_px / convex_area_px
    m.solidity = (m.convex_area_px > 0)
        ? m.area_px / static_cast<double>(m.convex_area_px)
        : 0.0;

    // Convexity: convex_hull_perimeter / real_perimeter
    m.convexity = (m.perimeter_px > 0.0)
        ? convex_perimeter_px / m.perimeter_px
        : 0.0;

    // Rotation-aware ellipse axes via fitEllipse
    bool ellipse_ok = computeEllipseAxes(
        contour,
        k,
        m.major_axis_cm,
        m.minor_axis_cm,
        m.ellipse_angle
    );

    if (!ellipse_ok) {
        // fallback to OBB axes
        m.major_axis_cm = m.L_cm;
        m.minor_axis_cm = m.W_cm;
        m.ellipse_angle = obb.angle;
    }
    // FI = alpha * solidity + beta * convexity + gamma * AR
    m.fragment_index = computeFragmentIndex(
        m.solidity,
        m.convexity,
        m.aspect_ratio,
        fi_params
    );

    return m;
}

}