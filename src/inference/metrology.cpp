#include "inference/metrology.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <functional>
#include <iostream>


// granulometry curves D30/D50/D80,
// histogram computation, PCA + KMeans clustering

namespace  RockPulse {

    // Main - Entry - Point

    MetrologyReport MetrologyEngine::analyze(

        const std::vector<RockMetrics>&  rocks,  
        int  num_clusters
    )  {

        MetrologyReport report ;

        if ( rocks.empty() )  {

            return report ;
        }

        report.rock_count = static_cast<int>( rocks.size() ) ;

        // Granulometry Curve + Dx Values 
        report.granulometry = computeGranulometry( rocks ) ;

        // Histograms for key metrics 
        report.hist_equiv_diameter = computeHistogram(
            rocks, "equiv_diameter_cm", HISTOGRAM_BINS,
            []( const RockMetrics& r) {
                return r.equiv_diameter;
            }
        );

        report.hist_volume = computeHistogram(

            rocks, "volumen_cm3" , HISTOGRAM_BINS,
            []( const RockMetrics r) {
                return static_cast<double>( r.volume_cm3) ;
            }
        );
        
        report.hist_aspect_ratio = computeHistogram(

            rocks, "aspet_ration",  HISTOGRAM_BINS,
            []( const RockMetrics r ) {
                
                return static_cast<double>( r.aspect_ratio);
            }
        );

        report.hist_sphericity =  computeHistogram(

            rocks, "sphericity" , HISTOGRAM_BINS,
            [] ( const RockMetrics& r) {
                return r.sphericity;
            }
        );

        //  PCA + KMEANS  Clustering 

        if ( static_cast<int>( rocks.size() ) >= num_clusters )  {

            cv::Mat feature_matrix = buildFeatureMatrix( rocks ) ;
            cv::PCA pca =  runPCA( 
                feature_matrix ,
                report.clustering
            );

            runKMeans(  feature_matrix,  pca,  num_clusters,  report.clustering  );

        }  else {

            std::cerr << "[ MetrologyEngine] Not Enoigh rocks for " << num_clusters << "clusters , then skipping." << std::endl;

        }
        // Job-level aggregates
        computeAggregates(rocks, report);

            return report;

    }

    GranulometryCurve MetrologyEngine::computeGranulometry(
        const std::vector<RockMetrics>& rocks 
    )   {

        GranulometryCurve curve ;

        struct DiamVol {

            double diam;
            double vol;
        };

        std::vector<DiamVol> dv ;
        dv.reserve( rocks.size() );

        for ( const auto& r : rocks ) {

            dv.push_back( {
                r.equiv_diameter,
                static_cast<double>( r.volume_cm3)
            });
        }

        std::sort( dv.begin()  , dv.end()   ,
        []( const DiamVol a, const DiamVol b ) {
            return a.diam < b.diam;
        } );

        // Cumulative Volume Sum 
        double total_vol = 0.0;

        for ( const auto& p : dv )  {

            total_vol = p.vol + total_vol;
        }

        if ( total_vol <= 0.0 ) {

            return curve ;

        }

        curve.diameters_cm.reserve(dv.size() ) ;
        curve.cumulative_passing.reserve(dv.size() ) ;

        double cumsum = 0.0;

        for ( const auto& p : dv ) {

            cumsum =  p.vol + cumsum;

            curve.diameters_cm.push_back( p.diam ) ;
            curve.cumulative_passing.push_back( cumsum / total_vol ) ;

        }

        // Interpolate Dx Values 
        curve.D30 = interpolateDx(

            curve.diameters_cm, curve.cumulative_passing, 0.30
        );
        curve.D50 = interpolateDx(

            curve.diameters_cm, curve.cumulative_passing, 0.50
        );
        curve.D80 = interpolateDx(

            curve.diameters_cm, curve.cumulative_passing, 0.80
        );

        return curve;

}

double MetrologyEngine::interpolateDx(

    const std::vector<double>& diameters_cm,
    const std::vector<double>& cumulative_passing,
    double                     x)
{
    if (diameters_cm.empty()) return 0.0;

    // If x is below first point or above last point
    if (x <= cumulative_passing.front()) return diameters_cm.front();
    if (x >= cumulative_passing.back())  return diameters_cm.back();

    // Find first index where cumulative_passing >= x
    auto it = std::lower_bound(

        cumulative_passing.begin(),
        cumulative_passing.end(),
        x
    );

    if (it == cumulative_passing.begin()) return diameters_cm.front();

    std::size_t i1 = std::distance(cumulative_passing.begin(), it);

    std::size_t i0 = i1 - 1;

    // Linear interpolation between i0 and i1
    double cp0 = cumulative_passing[i0];
    double cp1 = cumulative_passing[i1];
    double d0  = diameters_cm[i0];
    double d1  = diameters_cm[i1];

    double t = (x - cp0) / (cp1 - cp0);

    return d0 + t * (d1 - d0);

}

Histogram MetrologyEngine::computeHistogram(

    const std::vector<RockMetrics>& rocks,
    const std::string&      metric_name,
    int          num_bins,
    std::function<double(const RockMetrics&)> selector
)  {
    Histogram h;
    h.metric_name = metric_name;

    if (rocks.empty() || num_bins <= 0) return h;

    // Extract values via selector
    std::vector<double> values;
    values.reserve(rocks.size());

    for (const auto& r : rocks) {

        values.push_back(selector(r));
    }

    // Range
    h.min = *std::min_element(values.begin(), values.end());
    h.max = *std::max_element(values.begin(), values.end());

    // Mean and stddev
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    h.mean     = sum / static_cast<double>(values.size());

    double sq_sum = 0.0;

    for (double v : values) {

        sq_sum += (v - h.mean) * (v - h.mean);

    }

    h.stddev = std::sqrt(sq_sum / static_cast<double>(values.size()));

    // Build bin edges
    double range    = h.max - h.min;
    double bin_width = (range > 0.0)
        ? range / static_cast<double>(num_bins)
        : 1.0;

    h.bin_edges.resize(num_bins + 1);
    for (int i = 0; i <= num_bins; ++i) {
        h.bin_edges[i] = h.min + i * bin_width;
    }

    // Count values per bin
    h.counts.assign(num_bins, 0);
    for (double v : values) {
        int bin = static_cast<int>((v - h.min) / bin_width);
        bin = std::max(0, std::min(bin, num_bins - 1));
        h.counts[bin]++;
    }

    return h;
}

// Shape: (n_rocks, 6) - normalized to [0,1]
cv::Mat MetrologyEngine::buildFeatureMatrix(
    const std::vector<RockMetrics>& rocks)
{
    const int n_rocks    = static_cast<int>(rocks.size());
    const int n_features = 6;

    cv::Mat matrix(n_rocks, n_features, CV_32F);

    for (int i = 0; i < n_rocks; ++i) {

        const auto& r = rocks[i];

        matrix.at<float>(i, 0) = static_cast<float>(r.equiv_diameter);
        matrix.at<float>(i, 1) = static_cast<float>(r.sphericity);
        matrix.at<float>(i, 2) = r.aspect_ratio;
        matrix.at<float>(i, 3) = static_cast<float>(r.convexity);
        matrix.at<float>(i, 4) = static_cast<float>(r.solidity);
        matrix.at<float>(i, 5) = r.volume_cm3;
    }

    normalizeMatrix(matrix);

    return matrix;

}

cv::Mat MetrologyEngine::buildFeatureMatrix(

    const std::vector<RockMetrics>& rocks)
{
    const int n_rocks    = static_cast<int>(rocks.size());
    const int n_features = 6;

    cv::Mat matrix(n_rocks, n_features, CV_32F);

    for (int i = 0; i < n_rocks; ++i) {

        const auto& r = rocks[i];

        matrix.at<float>(i, 0) = static_cast<float>(r.equiv_diameter);
        matrix.at<float>(i, 1) = static_cast<float>(r.sphericity);
        matrix.at<float>(i, 2) = r.aspect_ratio;
        matrix.at<float>(i, 3) = static_cast<float>(r.convexity);
        matrix.at<float>(i, 4) = static_cast<float>(r.solidity);
        matrix.at<float>(i, 5) = r.volume_cm3;
    }

    normalizeMatrix(matrix);

    return matrix;

}

void MetrologyEngine::normalizeMatrix(cv::Mat& matrix)
{
    for (int col = 0; col < matrix.cols; ++col) {

        cv::Mat column = matrix.col(col);
        double col_min, col_max;
        cv::minMaxLoc(column, &col_min, &col_max);

        double range = col_max - col_min;

        if (range > 1e-9) {
            
            column = (column - col_min) / range;

        } else {

            column.setTo(0.0f);
        }
    }
}
// Projects to 2D. Returns cv::PCA for centroid projection.
cv::PCA MetrologyEngine::runPCA(

    const cv::Mat& feature_matrix,
    ClusterResult& out_result
)  {
    // Retain 2 principal components for 2D scatter plot
    cv::PCA pca(feature_matrix, cv::Mat(), cv::PCA::DATA_AS_ROW, 2);

    cv::Mat projected = pca.project(feature_matrix);

    const int n = projected.rows;
    out_result.pca_x.resize(n);
    out_result.pca_y.resize(n);

    for (int i = 0; i < n; ++i) {

        out_result.pca_x[i] = static_cast<double>(
            projected.at<float>(i, 0));
        out_result.pca_y[i] = static_cast<double>(
            projected.at<float>(i, 1));
    }

    // Explained variance ratio: eigenvalue / sum(eigenvalues)
    cv::Mat eigenvalues = pca.eigenvalues;
    double total_var = cv::sum(eigenvalues)[0];

    if (total_var > 1e-9) {

        out_result.pca_variance_pc1 =
            eigenvalues.at<float>(0, 0) / total_var;
        out_result.pca_variance_pc2 =
            eigenvalues.at<float>(1, 0) / total_var;
    }

    return pca;

}

void MetrologyEngine::runKMeans(

    const cv::Mat&  feature_matrix,
    const cv::PCA&  pca,
    int             num_clusters,
    ClusterResult&  out_result

)  {
    
    out_result.num_clusters = num_clusters;

    cv::Mat labels;
    cv::Mat centroids;

    cv::TermCriteria criteria(
        cv::TermCriteria::EPS + cv::TermCriteria::COUNT,
        KMEANS_MAX_ITER,
        KMEANS_EPSILON
    );

    out_result.compactness = cv::kmeans(
        feature_matrix,
        num_clusters,
        labels,
        criteria,
        KMEANS_ATTEMPTS,
        cv::KMEANS_PP_CENTERS,
        centroids
    );

    // Copy labels to output
    const int n = labels.rows;

    out_result.cluster_labels.resize(n);
    
    for (int i = 0; i < n; ++i) {

        out_result.cluster_labels[i] = labels.at<int>(i, 0);
    }

    // Project centroids through PCA for 2D scatter plot
    cv::Mat centroids_2d = pca.project(centroids);

    out_result.centroid_pca_x.resize(num_clusters);
    out_result.centroid_pca_y.resize(num_clusters);

    for (int k = 0; k < num_clusters; ++k) {

        out_result.centroid_pca_x[k] = static_cast<double>(

            centroids_2d.at<float>(k, 0));

        out_result.centroid_pca_y[k] = static_cast<double>(

            centroids_2d.at<float>(k, 1));

    }

}
// Names clusters based on mean sphericity and equiv_diameter
void MetrologyEngine::analyzeClusterProfiles(

    const std::vector<RockMetrics>& rocks,
    ClusterResult&       out_result

)  {

    const int k = out_result.num_clusters;

    if (k == 0 || out_result.cluster_labels.empty()) return;

    // Compute per-cluster mean sphericity and mean equiv_diameter
    std::vector<double> mean_sphericity(k, 0.0);
    std::vector<double> mean_diam(k,       0.0);
    std::vector<int>    cluster_count(k,   0);

    for (std::size_t i = 0; i < rocks.size(); ++i) {

        int label = out_result.cluster_labels[i];
        mean_sphericity[label] += rocks[i].sphericity;
        mean_diam[label]       += rocks[i].equiv_diameter;

        cluster_count[label]++;
    }

    for (int i = 0; i < k; ++i) {

        if (cluster_count[i] > 0) {
            mean_sphericity[i] /= cluster_count[i];
            mean_diam[i]       /= cluster_count[i];
        }
    }

    // Assign names based on profile heuristics
    out_result.cluster_names.resize(k);

    for (int i = 0; i < k; ++i) {

        if (mean_sphericity[i] >= 0.75) {

            out_result.cluster_names[i] = "Compact Fragment";

        } else if (mean_sphericity[i] >= 0.50) {

            out_result.cluster_names[i] = "Irregular Rock";

        } else {

            out_result.cluster_names[i] = "Elongated Debris";
        }
    }
}

void MetrologyEngine::computeAggregates(

    const std::vector<RockMetrics>& rocks,
    MetrologyReport&                out_report

)  { 

    if (rocks.empty()) return;

    double sum_vol    = 0.0;
    double sum_mass   = 0.0;
    double sum_diam   = 0.0;
    double sum_sph    = 0.0;
    double sum_ar     = 0.0;

    for (const auto& r : rocks) {
        sum_vol  += r.volume_cm3;
        sum_mass += r.mass_g;
        sum_diam += r.equiv_diameter;
        sum_sph  += r.sphericity;
        sum_ar   += r.aspect_ratio;
    }

    const double n          = static_cast<double>(rocks.size());
    out_report.total_volume_cm3  = sum_vol;
    out_report.total_mass_g      = sum_mass;
    out_report.mean_equiv_diam   = sum_diam / n;
    out_report.mean_sphericity   = sum_sph  / n;
    out_report.mean_aspect_ratio = sum_ar   / n;
}

// end of RockPulse namespace
}
