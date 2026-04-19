#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/ml.hpp>
#include <vector>
#include <string>
#include <array>

#include "inference/metrics_calculator.hpp"

// It handles the metrology calculations for a single rock.
// granulometry curves D30/D50/D80,
// histogram computation, PCA + KMeans clustering input
// Operates on vector<RockMetrics> delivered by RockDetector


namespace RockPulse {

// Cumulative passing curve by equivalent diameter.
// Each entry: { D_eq_cm, cumulative_volume_fraction [0,1] }
struct GranulometryCurve {
std::vector<double> diameters_cm;       // sorted ascending
std::vector<double> cumulative_passing; // [0.0 - 1.0]
double   D30 = 0.0;
double   D50 = 0.0;
double   D80 = 0.0;
};

// Frequency distribution of a single metric across all rocks.
// bin_edges has (num_bins + 1) entries.
// counts[i] = rocks with metric in [bin_edges[i], bin_edges[i+1])
struct Histogram {
    std::string         metric_name;
    std::vector<double> bin_edges;
    std::vector<int>    counts;
    double    mean   = 0.0;
    double    stddev = 0.0;
    double    min    = 0.0;
    double    max    = 0.0;
};
// Output of KMeans clustering over rock feature matrix.
// cluster_labels[i] = cluster id assigned to rocks[i]
// centroids: one row per cluster, columns = feature order
struct ClusterResult {
    std::vector<int>    cluster_labels;   // size == rocks.size()
    int     num_clusters = 3;
    double  compactness  = 0.0;

    // Centroid coords in PCA-reduced 2D space for scatter plot
    std::vector<double> centroid_pca_x;
    std::vector<double> centroid_pca_y;

    // PCA-projected coordinates per rock for scatter plot
    std::vector<double> pca_x;
    std::vector<double> pca_y;

    // Explained variance ratio of PC1 and PC2
    double   pca_variance_pc1 = 0.0;
    double   pca_variance_pc2 = 0.0;

    // Assigned post-hoc by analyzeClusterProfiles()
    std::vector<std::string> cluster_names;
};
// Aggregated output for one detection job.
// Serialized directly to JSON by the Crow detect handler.
struct MetrologyReport {
    // Granulometry
    GranulometryCurve   granulometry;

    // Histograms - one per key metric
    Histogram    hist_equiv_diameter;
    Histogram    hist_volume;
    Histogram    hist_sphericity;
    Histogram    hist_aspect_ratio;

    // Clustering
    ClusterResult    clustering;

    // Job-level aggregates
    double     total_volume_cm3  = 0.0;
    double     total_mass_g      = 0.0;
    double      mean_equiv_diam   = 0.0;
    double      mean_sphericity   = 0.0;
    double      mean_aspect_ratio = 0.0;
    int         rock_count        = 0;
};
// Stateless. All methods static.
// Operates on const vector<RockMetrics>& - no mutation.
class MetrologyEngine {
public:
    // Main entry point. Computes full MetrologyReport.
    // Parameters:
    //   rocks        - vector<RockMetrics> from RockDetector
    //   num_clusters - K for KMeans (default 3)
    static MetrologyReport analyze(

        const std::vector<RockMetrics>& rocks, int num_clusters = 3 ) ;

        // Builds cumulative volume passing curve.
        // Sorts rocks by equiv_diameter ascending.
        // Interpolates D30, D50, D80 from cumulative volume.
        static GranulometryCurve computeGranulometry(
            const std::vector<RockMetrics>& rocks 
        );
        // Generic histogram over any scalar metric extracted
        // via a lambda selector.
        static Histogram computeHistogram (

            const std::vector<RockMetrics>& rocks, 
            const std::string&  metric_name ,
            int num_bins ,
            std::function< double (  const RockMetrics& ) > selector
        );
        // Assembles cv::Mat of shape (n_rocks, n_features) for
        // PCA and KMeans. Features are normalized to [0,1].
        // Feature Order :  [0] : D_eq,  [1]: shericity, [2]: aspect_ration , 
        // [3]: convexity, [4]: solidity: [5]: volume_cm3]
        static cv::Mat buildFeatureMatrix( 
            const std::vector< RockMetrics >& rocks 
        );
        // Projects feature matrix to 2D using cv::PCA.
        // Writes projected coords into ClusterResult.
        // Returns the cv::PCA object for centroid projection.
        static cv::PCA runPCA(
        const cv::Mat&  feature_matrix,
        ClusterResult&  out_result
        );
        // Runs cv::kmeans on feature matrix.
        // Writes cluster_labels and compactness into ClusterResult.
        // Projects centroids through PCA for scatter plot.
        static void runKMeans(
        const cv::Mat&  feature_matrix,
        const cv::PCA&  pca,
        int             num_clusters,
        ClusterResult&  out_result
        );
        // Assigns descriptive - readable names to clusters based on
        // centroid values of sphericity and equiv_diameter.
        static void analyzeClusterProfiles(
        const std::vector<RockMetrics>& rocks,
        ClusterResult&     out_result
        );
        // Linear interpolation to find diameter at which x%
        // of cumulative volume has passed.
        // x in [0.0, 1.0] (e.g. 0.50 for D50)
        static double interpolateDx(
        const std::vector<double>& diameters_cm,
        const std::vector<double>& cumulative_passing,
        double    x
        );

private:

    MetrologyEngine() = delete ; 

    // Min-Max Normalization  / column in-place

    static void normalizeMatrix ( cv::Mat& matrix );

    // Fill-Job_level aggregate fileds in MetrologyReport
    static void computeAggregates( 
        
        const std::vector<RockMetrics>& rocks,
        MetrologyReport& out_report
    );

    static constexpr int KMEANS_ATTEMPTS = 10;
    static constexpr  int KMEANS_MAX_ITER  = 100;
    static constexpr double KMEANS_EPSILON  = 0.001;
    static constexpr int HISTOGRAM_BINS     = 20;

};

// end of RockPulse namespace 

}