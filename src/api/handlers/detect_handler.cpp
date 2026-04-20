#include "api/handlers/detect_handler.hpp"

#include <crow/multipart.h>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace RockPulse {

// Private helpers — not exposed outside this translation unit.
namespace {

std::string generateUUID()
{
    static thread_local std::mt19937 rng(std::random_device{}());
    static thread_local std::uniform_int_distribution<int> dist(0, 15);
    static thread_local std::uniform_int_distribution<int> dist2(8, 11);

    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8;  ++i) ss << dist(rng);
    ss << "-";
    for (int i = 0; i < 4;  ++i) ss << dist(rng);
    ss << "-4";
    for (int i = 0; i < 3;  ++i) ss << dist(rng);
    ss << "-";
    ss << dist2(rng);
    for (int i = 0; i < 3;  ++i) ss << dist(rng);
    ss << "-";
    for (int i = 0; i < 12; ++i) ss << dist(rng);
    return ss.str();
}

std::string saveInputImage(
    const std::string& inputs_dir,
    const std::string& job_id,
    const std::string& image_bytes)
{
    const std::string path = inputs_dir + "/" + job_id + ".jpg";
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "[detect] Failed to save input image: " << path << "\n";
        return "";
    }
    ofs.write(image_bytes.data(),
              static_cast<std::streamsize>(image_bytes.size()));
    return path;
}

std::string saveOutputImage(
    const std::string& outputs_dir,
    const std::string& job_id,
    const cv::Mat&     annotated_image)
{
    const std::string path = outputs_dir + "/" + job_id + ".png";
    if (!cv::imwrite(path, annotated_image)) {
        std::cerr << "[detect] Failed to save output image: " << path << "\n";
        return "";
    }
    return path;
}

crow::json::wvalue rockToJson(const RockMetrics& r)
{
    crow::json::wvalue rock;
    rock["id"]             = r.rock_id;
    rock["confidence"]     = static_cast<double>(r.confidence);
    rock["area_cm2"]       = r.area_cm2;
    rock["L_cm"]           = static_cast<double>(r.L_cm);
    rock["W_cm"]           = static_cast<double>(r.W_cm);
    rock["estimate_H_cm"]  = static_cast<double>(r.estimate_H_cm);
    rock["volume_cm3"]     = static_cast<double>(r.volume_cm3);
    rock["mass_g"]         = static_cast<double>(r.mass_g);
    rock["perimeter_cm"]   = r.perimeter_cm;
    rock["equiv_diameter"] = r.equiv_diameter;
    rock["aspect_ratio"]   = static_cast<double>(r.aspect_ratio);
    rock["sphericity"]     = r.sphericity;
    rock["convexity"]      = r.convexity;
    rock["solidity"]       = r.solidity;
    rock["major_axis_cm"]  = static_cast<double>(r.major_axis_cm);
    rock["minor_axis_cm"]  = static_cast<double>(r.minor_axis_cm);
    rock["ellipse_angle"]  = static_cast<double>(r.ellipse_angle);

    if (r.fragment_index >= 0.0f) {
        rock["fragment_index"] = static_cast<double>(r.fragment_index);
    } else {
        rock["fragment_index"] = nullptr;
    }

    crow::json::wvalue bbox;
    bbox["x"] = r.bbox_x;
    bbox["y"] = r.bbox_y;
    bbox["w"] = r.bbox_w;
    bbox["h"] = r.bbox_h;
    rock["bbox"] = std::move(bbox);

    return rock;
}

crow::json::wvalue buildDetectResponse(
    const DetectionResult&  result,
    const MetrologyReport&  report,
    const std::string&      job_id,
    const std::string&      conveyor_id,
    const std::string&      location_code)
{
    crow::json::wvalue body;
    body["job_id"]        = job_id;
    body["conveyor_id"]   = conveyor_id;
    body["location_code"] = location_code;
    body["rock_count"]    = result.rock_count;

    std::vector<crow::json::wvalue> detections;
    detections.reserve(result.rocks.size());
    for (const auto& r : result.rocks) {
        detections.push_back(rockToJson(r));
    }
    body["detections"] = std::move(detections);

    crow::json::wvalue aggregates;
    aggregates["total_volume_cm3"]  = report.total_volume_cm3;
    aggregates["total_mass_g"]      = report.total_mass_g;
    aggregates["mean_equiv_diam"]   = report.mean_equiv_diam;
    aggregates["mean_sphericity"]   = report.mean_sphericity;
    aggregates["mean_aspect_ratio"] = report.mean_aspect_ratio;
    body["aggregates"] = std::move(aggregates);

    crow::json::wvalue gran;
    gran["D30"] = report.granulometry.D30;
    gran["D50"] = report.granulometry.D50;
    gran["D80"] = report.granulometry.D80;

    std::vector<crow::json::wvalue> curve_pts;
    curve_pts.reserve(report.granulometry.diameters_cm.size());
    for (std::size_t i = 0; i < report.granulometry.diameters_cm.size(); ++i) {
        crow::json::wvalue pt;
        pt["d"]  = report.granulometry.diameters_cm[i];
        pt["cp"] = report.granulometry.cumulative_passing[i];
        curve_pts.push_back(std::move(pt));
    }
    gran["curve"] = std::move(curve_pts);
    body["granulometry"] = std::move(gran);

    auto serializeHistogram = [](const Histogram& h) -> crow::json::wvalue {
        crow::json::wvalue hj;
        hj["metric"] = h.metric_name;
        hj["mean"]   = h.mean;
        hj["stddev"] = h.stddev;
        hj["min"]    = h.min;
        hj["max"]    = h.max;

        std::vector<crow::json::wvalue> bins;
        bins.reserve(h.counts.size());
        for (std::size_t i = 0; i < h.counts.size(); ++i) {
            crow::json::wvalue bin;
            bin["from"]  = h.bin_edges[i];
            bin["to"]    = h.bin_edges[i + 1];
            bin["count"] = h.counts[i];
            bins.push_back(std::move(bin));
        }
        hj["bins"] = std::move(bins);
        return hj;
    };

    crow::json::wvalue histograms;
    histograms["equiv_diameter"] = serializeHistogram(report.hist_equiv_diameter);
    histograms["volume"]         = serializeHistogram(report.hist_volume);
    histograms["sphericity"]     = serializeHistogram(report.hist_sphericity);
    histograms["aspect_ratio"]   = serializeHistogram(report.hist_aspect_ratio);
    body["histograms"] = std::move(histograms);

    crow::json::wvalue clustering;
    clustering["num_clusters"]     = report.clustering.num_clusters;
    clustering["compactness"]      = report.clustering.compactness;
    clustering["pca_variance_pc1"] = report.clustering.pca_variance_pc1;
    clustering["pca_variance_pc2"] = report.clustering.pca_variance_pc2;

    std::vector<crow::json::wvalue> scatter;
    scatter.reserve(result.rocks.size());
    for (std::size_t i = 0; i < result.rocks.size(); ++i) {
        crow::json::wvalue pt;
        pt["rock_id"] = result.rocks[i].rock_id;
        pt["x"]       = report.clustering.pca_x[i];
        pt["y"]       = report.clustering.pca_y[i];
        pt["cluster"] = report.clustering.cluster_labels[i];
        if (i < report.clustering.cluster_names.size()) {
            pt["cluster_name"] =
                report.clustering.cluster_names[
                    report.clustering.cluster_labels[i]
                ];
        }
        scatter.push_back(std::move(pt));
    }
    clustering["scatter"] = std::move(scatter);

    std::vector<crow::json::wvalue> centroids;
    centroids.reserve(static_cast<std::size_t>(report.clustering.num_clusters));
    for (int k = 0; k < report.clustering.num_clusters; ++k) {
        crow::json::wvalue c;
        c["cluster"] = k;
        c["x"]       = report.clustering.centroid_pca_x[k];
        c["y"]       = report.clustering.centroid_pca_y[k];
        if (k < static_cast<int>(report.clustering.cluster_names.size())) {
            c["name"] = report.clustering.cluster_names[k];
        }
        centroids.push_back(std::move(c));
    }
    clustering["centroids"] = std::move(centroids);
    body["clustering"] = std::move(clustering);

    return body;
}

} // anonymous namespace

namespace Handlers {

crow::response detect(
    const crow::request&        req,
    AuthMiddleware::context&    auth_ctx,
    RockDetector&               detector,
    PostgresConnector&          db,
    const ServerConfig&         cfg)
{
    crow::multipart::message multipart_msg(req);

    const auto image_part_it = multipart_msg.part_map.find("image");
    if (image_part_it == multipart_msg.part_map.end()) {
        crow::json::wvalue err;
        err["error"] = "Missing 'image' field in multipart body.";
        return crow::response(400, err);
    }

    const std::string& image_bytes = image_part_it->second.body;
    if (image_bytes.empty()) {
        crow::json::wvalue err;
        err["error"] = "Empty image payload.";
        return crow::response(400, err);
    }

    std::vector<uchar> buf(image_bytes.begin(), image_bytes.end());
    cv::Mat image = cv::imdecode(buf, cv::IMREAD_COLOR);
    if (image.empty()) {
        crow::json::wvalue err;
        err["error"] = "Failed to decode image. Supported: JPEG, PNG, BMP.";
        return crow::response(422, err);
    }

    const std::string  job_id   = generateUUID();
    const ConveyorRow& conveyor = auth_ctx.token_validation.conveyor;
    const LocationRow& location = auth_ctx.token_validation.location;

    float k = 0.0f;
    if (conveyor.calibration_marker_cm > 0.0f) {
        // Placeholder: ArUco marker detection will override this
        k = conveyor.calibration_marker_cm / 640.0f;
    }

    DetectionJob job;
    job.job_id          = job_id;
    job.image           = std::move(image);
    job.k               = k;
    job.density         = conveyor.base_rock_density;
    job.fi_params.alpha = location.fi_alpha;
    job.fi_params.beta  = location.fi_beta;
    job.fi_params.gamma = location.fi_gamma;

    const std::string input_path =
        saveInputImage(cfg.inputs_dir, job_id, image_bytes);

    DetectionResult det_result = detector.detect(job);
    if (!det_result.success) {
        crow::json::wvalue err;
        err["error"]  = "Inference failed.";
        err["detail"] = det_result.error_msg;
        return crow::response(500, err);
    }

    MetrologyReport report = MetrologyEngine::analyze(det_result.rocks, 3);

    const std::string output_path =
        saveOutputImage(cfg.outputs_dir, job_id, det_result.output_image);

    const std::string db_job_id = db.insertDetectionJob(
        conveyor.conveyor_id,
        det_result.rock_count,
        k,
        conveyor.base_rock_density,
        input_path,
        output_path
    );

    db.insertRockDetections(db_job_id, det_result.rocks);
    db.insertClusterResults(db_job_id, report.clustering);

    crow::json::wvalue body = buildDetectResponse(
        det_result, report, db_job_id,
        conveyor.conveyor_id, location.location_code
    );

    crow::response res(200, body);
    res.set_header("Content-Type", "application/json");
    return res;
}

} // namespace Handlers
} // namespace RockPulse
