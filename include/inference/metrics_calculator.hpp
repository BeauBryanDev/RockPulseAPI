#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>


namespace RockPulse {

    // RockMetrics
    // It Holds all computed metrics for a single detected rock.
    // Populated by MetricsCalculator::compute()

    struct RockMetrics {

        int  rock_id  = 0;

        // Bounding Box 
        int  bbox_x   = 0;
        int  bbox_y  = 0;
        int  bbox_w  = 0;
        int  bbox_h    = 0;
        float   confidence   = 0.0f;

        // Pixel Space -- raw mesurments
        double  area_px         = 0.0;   // countNonZero(binMask)
        double  perimeter_px    = 0.0;   // arcLength(contour, true)
        float   L_px            = 0.0f;  // major axis - minAreaRect
        float   W_px            = 0.0f;  // minor axis - minAreaRect
        int     convex_area_px  = 0;     // convexHull pixel count

        // Real World Mesurements ( cm ) 

        double  area_cm2        = 0.0;
        double  perimeter_cm    = 0.0;
        float   L_cm            = 0.0f;
        float   W_cm            = 0.0f;
        float   estimate_H_cm   = 0.0f;  // (2/5) * (L_cm + W_cm)
        double  equiv_diameter  = 0.0;   // sqrt(4 * area_cm2 / pi)

        // Volume and Mass
        float   volume_cm3      = 0.0f;  // (4* pi / 3) * ( L/2 ) *( W /2 )* ( H / 2 )
        float   mass_g          = 0.0f;  // density_kg_m3 * volume_cm3 * 0.001

        // Shape Descriptors 
        float   aspect_ratio    = 0.0f;  // L_cm / W_cm
        double  sphericity      = 0.0;   // Cox: (4*pi*area_px) / perimeter_px^2
        double  convexity       = 0.0;   // perimeter_convex / perimeter_real
        double  solidity        = 0.0;   // area_px / convex_area_px

        // Elipset Axes 
        float   major_axis_cm   = 0.0f;  // fitEllipse major axis * k
        float   minor_axis_cm   = 0.0f;  // fitEllipse minor axis * k
        float   ellipse_angle   = 0.0f;  // orientation angle (degrees)

        // Fragment Index 
        // FI = alpha * solidity + beta * convexity + gamma * aspect_ratio
        float   fragment_index  = -1.0f; // -1 = not yet calibrated

    };


    // FIParams
    struct FIParams {
    float alpha = 0.33f;  // weight for solidity
    float beta  = 0.33f;  // weight for convexity
    float gamma = 0.34f;  // weight for aspect_ratio
    
    };

// MetricsCalculator
// Stateless utility class. All methods are static.
class MetricsCalculator {
public:
// compute()
    // Main entry point. Takes a binary mask (CV_8U, single ROI)
    // and returns a fully populated RockMetrics struct.
static RockMetrics compute(
        const cv::Mat&      bin_mask, // CV_8U binary mask cropped to bbox ROI
        const cv::Rect&     bbox,  /// bounding box in original image coordinates
        float               confidence, // YOLOv8 detection confidence score
        int                 rock_id,  // Sequential id within this current detection job
        float               k,  // calibration factor ( cm / px ) , it came from ArcUco Market
        float               density,  // Conveyeor belt rock density Kg/m3
        const FIParams&     fi_params   // alpha , beta , gamma from particular inner location mining metrics 
    );

    static RockMetrics compute(
        const cv::Mat&      bin_mask,
        const cv::Rect&     bbox,
        float               confidence,
        int                 rock_id,
        float               k,
        float               density,
        const FIParams&     fi_params
    );
    // Uses cv::fitEllipse on the largest contour to get
    // rotation-aware major/minor axes. More precise than
    // minAreaRect for irregular rock shapes.
    // Returns false if contour has fewer than 5 points
    // (minimum required by fitEllipse).
    static bool computeEllipseAxes(
        const std::vector<cv::Point>& contour,
        float   k,
        float&  out_major_cm,
        float&  out_minor_cm,
        float&  out_angle_deg
    );
    // Computes convex hull area (px) and perimeter (px).
    // Used for solidity and convexity
    static void computeConvexMetrics(
        const std::vector<cv::Point>& contour,
        int&    out_convex_area_px,
        double& out_convex_perimeter_px
    );
    // FI = alpha * solidity + beta * convexity + gamma * aspect_ratio
    // Result is clamped to [0.0, 1.0].
    // Returns -1.0f if fi_params are default (not calibrated).
    static float computeFragmentIndex(
        double          solidity,
        double          convexity,
        float           aspect_ratio,
        const FIParams& fi_params
    );
    // Returns iterator to the contour with maximum area.
    // Used consistently across all metric computations
    // to ensure all metrics refer to the same contour.
    static std::vector<std::vector<cv::Point>>::const_iterator largestContour(
        const std::vector<std::vector<cv::Point>>& contours
    );

private:
    MetricsCalculator() = delete;  // pure static, no instantiation
};

// end of RockPulse namespace 

}