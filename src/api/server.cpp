#include "api/server.hpp"
#include <crow/multipart.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>


namespace fs = std::filesystem;

namespace RockPulse {

    // CorsMiddleware

    void CorsMiddleware::before_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx)
    {
    // Handle OPTIONS preflight from React frontend
    if (req.method == crow::HTTPMethod::Options) {

        res.code = 204;

        res.add_header("Access-Control-Allow-Origin",  "*");

        res.add_header("Access-Control-Allow-Methods",
                       "GET, POST, OPTIONS");

        res.add_header("Access-Control-Allow-Headers",
                       "Content-Type, X-API-KEY, Authorization");

        res.add_header("Access-Control-Max-Age", "86400");

        res.end();
    }
}

void CorsMiddleware::after_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx

)  {
    res.add_header("Access-Control-Allow-Origin",  "*");

    res.add_header("Access-Control-Allow-Methods",
                   "GET, POST, OPTIONS");

    res.add_header("Access-Control-Allow-Headers",
                   "Content-Type, X-API-KEY, Authorization");

}

// AuthMiddleware
void AuthMiddleware::before_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx)
    {
    // Health endpoint is public - skip auth
    if (req.url == "/api/health") return;

    const std::string raw_token = req.get_header_value("X-API-KEY");

    if (raw_token.empty()) {
        res.code = 401;
        res.write(R"({"error":"Missing X-API-KEY header."})");
        res.end();
        return;
    }

    ctx.token_validation = db->validateToken(raw_token);

    if (!ctx.token_validation.valid) {
        res.code = 401;
        res.write(
            R"({"error":")" +
            ctx.token_validation.error_msg +
            R"("})"
        );
        res.end();
    }
}

void AuthMiddleware::after_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx)
{
    // No-op
}

// Server - constructor
Server::Server(
    const ServerConfig&   server_cfg,
    const DBConfig&       db_cfg,
    const DetectorConfig& detector_cfg
)
    : server_cfg_(server_cfg)
{
    // Create shared resources - order matters
    db_       = std::make_shared<PostgresConnector>(db_cfg);
    detector_ = std::make_shared<RockDetector>(detector_cfg);

    // Inject db into AuthMiddleware before routes are registered
    app_.get_middleware<AuthMiddleware>().db = db_;

    // Ensure output directories exist
    fs::create_directories(server_cfg_.outputs_dir);
    fs::create_directories(server_cfg_.inputs_dir);

    registerRoutes();

    std::cout << "[Server] RockPulseAPI initialized."  << std::endl;

    std::cout << "[Server] Detector ready : "
              << (detector_->isReady() ? "YES" : "NO") << std::endl;

    std::cout << "[Server] DB ready       : "
              << (db_->isReady()       ? "YES" : "NO") << std::endl;
}

bool Server::isReady() const
{
    return detector_->isReady() && db_->isReady();
}

void Server::run()
{
    std::cout << "[Server] Listening on port "
              << server_cfg_.port << std::endl;

    app_.port(server_cfg_.port)

        .concurrency(server_cfg_.concurrency)
        .run();
}

void Server::registerRoutes() {



    // GET /api/health  (public - no auth)

    CROW_ROUTE(app_, "/api/health")
        .methods(crow::HTTPMethod::Get)
        ([this](const crow::request& req) {
            return handleHealth(req);
        });


    // POST /api/v1/detect  (protected)

    CROW_ROUTE(app_, "/api/v1/detect")
        .methods(crow::HTTPMethod::Post)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](
            const crow::request& req,
            crow::response&      res,
            AuthMiddleware::context& auth_ctx)
        {
            res = handleDetect(req, auth_ctx);
            res.end();
        });



    // GET /api/v1/detections  (protected)

    CROW_ROUTE(app_, "/api/v1/detections")
        .methods(crow::HTTPMethod::Get)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](
            const crow::request& req,
            crow::response&      res,
            AuthMiddleware::context& auth_ctx)
        {
            res = handleListDetections(req, auth_ctx);
            res.end();
        });


    // GET /api/v1/detections/<job_id>  (protected)

    CROW_ROUTE(app_, "/api/v1/detections/<string>")
        .methods(crow::HTTPMethod::Get)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](
            const crow::request& req,
            crow::response&      res,
            const std::string&   job_id,
            AuthMiddleware::context& auth_ctx)
        {
            res = handleGetDetection(req, job_id, auth_ctx);
            res.end();
        });


    }

// handleHealth()
// GET /api/health
crow::response Server::handleHealth(

    const crow::request& req
)  {
    crow::json::wvalue body;
    body["status"]  = isReady() ? "ok" : "degraded";
    body["model"]    = detector_->modelName();
    body["db"]      = db_->isReady() ? "connected" : "error";
    body["detector"]  = detector_->isReady() ? "ready" : "error";
    body["version"]  = "1.0.0";

    crow::response res(200, body);
    res.set_header("Content-Type", "application/json");
    return res;
}

// handleDetect()
// POST /api/v1/detect
crow::response Server::handleDetect(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx
)   {

    // Parse multipart/form-data

    crow::multipart::message multipart_msg(req);

    const auto image_part_it =
        multipart_msg.part_map.find("image");

    if (image_part_it == multipart_msg.part_map.end()) {

        crow::json::wvalue err;
        err["error"] = "Missing 'image' field in multipart body.";
        return crow::response(400, err);
    }

    const std::string& image_bytes =
        image_part_it->second.body;

    if (image_bytes.empty()) {

        crow::json::wvalue err;
        err["error"] = "Empty image payload.";

        return crow::response(400, err);
    }


    //  Decode image bytes to cv::Mat

    std::vector<uchar> buf(
        image_bytes.begin(),
        image_bytes.end()
    );

    cv::Mat image = cv::imdecode(buf, cv::IMREAD_COLOR);

    if (image.empty()) {

        crow::json::wvalue err;
        err["error"] = "Failed to decode image. "
                       "Supported: JPEG, PNG, BMP.";
        return crow::response(422, err);

    }

    // Build job_id and load conveyor context from auth ctx

    const std::string job_id = generateUUID();

    const ConveyorRow& conveyor = auth_ctx.token_validation.conveyor;
    const LocationRow& location = auth_ctx.token_validation.location;

    // Calibration k: marker_cm / marker_px
    // For now k is loaded from conveyor config.
    // ArUco detection can override this value in future iteration.
    float k = 0.0f;

    if (conveyor.calibration_marker_cm > 0.0f) {
        // Assume marker occupies fixed reference px at 640px input
        // This will be replaced by ArUco marker detection later 
        k = conveyor.calibration_marker_cm / 640.0f;
        // TODO :  CAREFULL WITH ArUco  marker detectro objeto : px -> cm
    }

    // Assemble DetectionJob

    DetectionJob job;
    job.job_id  = job_id;
    job.image   = std::move(image);
    job.k       = k;
    job.density = conveyor.base_rock_density;
    job.fi_params.alpha = location.fi_alpha;
    job.fi_params.beta  = location.fi_beta;
    job.fi_params.gamma = location.fi_gamma;

    //  Save input image

    const std::string input_path =
        saveInputImage(job_id, image_bytes);


    //  Run inference pipeline

    DetectionResult det_result = detector_->detect(job);

    if (!det_result.success) {

        crow::json::wvalue err;
        err["error"]  = "Inference failed.";
        err["detail"] = det_result.error_msg;
        return crow::response(500, err);
    }

    //  Run metrology analysis

    MetrologyReport report = MetrologyEngine::analyze(
        det_result.rocks, 3
    );

    //  Save annotated output image

    const std::string output_path =
        saveOutputImage(job_id, det_result.output_image);


    // Save in db  to PostgreSQL

    const std::string db_job_id = db_->insertDetectionJob(
        conveyor.conveyor_id,
        det_result.rock_count,
        k,
        conveyor.base_rock_density,
        input_path,
        output_path
    );

    db_->insertRockDetections(db_job_id, det_result.rocks);
    db_->insertClusterResults(db_job_id, report.clustering);


    // Build and return JSON response

    crow::json::wvalue body = buildDetectResponse(
        det_result,
        report,
        db_job_id,
        conveyor.conveyor_id,
        location.location_code
    );

    crow::response res(200, body);
    res.set_header("Content-Type", "application/json");

    return res;

}

// handleListDetections()
// GET /api/v1/detections
crow::response Server::handleListDetections(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx)
{
    const std::string& conveyor_id =
        auth_ctx.token_validation.conveyor.conveyor_id;

    // Optional pagination query params
    int limit  = 20;
    int offset = 0;

    const auto limit_param  = req.url_params.get("limit");
    const auto offset_param = req.url_params.get("offset");

    if (limit_param)  limit  = std::stoi(limit_param);

    if (offset_param) offset = std::stoi(offset_param);

    // Clamp to safe range
    limit  = std::max(1,  std::min(limit,  100));
    offset = std::max(0,  offset);

    try {
        pqxx::result rows = db_->listDetectionJobs(
            conveyor_id, limit, offset
        );

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> jobs;
        jobs.reserve(rows.size());

        for (const auto& row : rows) {
            crow::json::wvalue job;
            job["job_id"]     = row["job_id"].as<std::string>();
            job["created_at"] = row["created_at"].as<std::string>();
            job["rock_count"] = row["rock_count"].as<int>();
            job["image_path"] = row["image_path"].as<std::string>();
            job["output_path"]= row["output_path"].as<std::string>();
            job["density"]    = row["density"].as<double>();
            jobs.push_back(std::move(job));
        }

        body["jobs"]   = std::move(jobs);
        body["limit"]  = limit;
        body["offset"] = offset;

        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to list detections.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }

}

// handleGetDetection()
// GET /api/v1/detections/<job_id>
crow::response Server::handleGetDetection(
    const crow::request&     req,
    const std::string&       job_id,
    AuthMiddleware::context& auth_ctx

)  {

    if (job_id.empty()) {

        crow::json::wvalue err;
        err["error"] = "job_id is required.";
        return crow::response(400, err);
    }

    try {

        pqxx::result rows = db_->getDetectionJob(job_id);

        if (rows.empty()) {

            crow::json::wvalue err;
            err["error"] = "Detection job not found: " + job_id;
            return crow::response(404, err);
        }

        crow::json::wvalue body;
        body["job_id"]     = job_id;
        body["rock_count"] = static_cast<int>(rows.size());

        std::vector<crow::json::wvalue> detections;
        detections.reserve(rows.size());

        for (const auto& row : rows) {

            crow::json::wvalue rock;

            rock["id"]             = row["id"].as<long long>();
            rock["confidence"]     = row["confidence"].as<double>();
            rock["area_cm2"]       = row["area_cm2"].as<double>();
            rock["L_cm"]           = row["L_cm"].as<double>();
            rock["W_cm"]           = row["W_cm"].as<double>();
            rock["estimate_H_cm"]  = row["estimate_H_cm"].as<double>();
            rock["volume_cm3"]     = row["volume_cm3"].as<double>();
            rock["mass_g"]         = row["mass_g"].as<double>();
            rock["sphericity"]     = row["sphericity"].as<double>();
            rock["aspect_ratio"]   = row["aspect_ratio"].as<double>();
            rock["convexity"]      = row["convexity"].as<double>();
            rock["solidity"]       = row["solidity"].as<double>();
            rock["equiv_diameter"] = row["equiv_diameter"].as<double>();
            rock["perimeter_cm"]   = row["perimeter_cm"].as<double>();
            rock["major_axis_cm"]  = row["major_axis_cm"].as<double>();
            rock["minor_axis_cm"]  = row["minor_axis_cm"].as<double>();

            // bbox
            crow::json::wvalue bbox;
            bbox["x"] = row["bbox_x"].as<int>();
            bbox["y"] = row["bbox_y"].as<int>();
            bbox["w"] = row["bbox_w"].as<int>();
            bbox["h"] = row["bbox_h"].as<int>();
            rock["bbox"] = std::move(bbox);

            // fragment_index - nullable
            if (!row["fragment_index"].is_null()) {

                rock["fragment_index"] =
                    row["fragment_index"].as<double>();

            } else {

                rock["fragment_index"] = nullptr;
            }

            detections.push_back(std::move(rock));
        }

        body["detections"] = std::move(detections);

        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");

        return res;

    } catch (const std::exception& e) {

        crow::json::wvalue err;
        err["error"]  = "Failed to retrieve detection.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// saveInputImage()
std::string Server::saveInputImage(

    const std::string& job_id,
    const std::string& image_bytes) const
{
    const std::string path =
        server_cfg_.inputs_dir + "/" + job_id + ".jpg";

    std::ofstream ofs(path, std::ios::binary);

    if (!ofs.is_open()) {

        std::cerr << "[Server] Failed to save input image: "
                  << path << std::endl;
        return "";
    }

    ofs.write(image_bytes.data(),
              static_cast<std::streamsize>(image_bytes.size())
            );

    return path;
}

// Save Output Imgages

std::string Server::saveOutputImage(

    const std::string& job_id,
    const cv::Mat&     annotated_image) const

{
    const std::string path =
        server_cfg_.outputs_dir + "/" + job_id + ".png";

    if (!cv::imwrite(path, annotated_image)) {

        std::cerr << "[Server] Failed to save output image: "
                  << path << std::endl;

        return "";
    }

    return path;
}

// rockToJson()
crow::json::wvalue Server::rockToJson(

    const RockMetrics& r) const
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

    // fragment_index sentinel -1 -> JSON null
    if (r.fragment_index >= 0.0f) {

        rock["fragment_index"] =
            static_cast<double>(r.fragment_index);

    } else {

        rock["fragment_index"] = nullptr;
    }

    // bbox
    crow::json::wvalue bbox;
    bbox["x"] = r.bbox_x;
    bbox["y"] = r.bbox_y;
    bbox["w"] = r.bbox_w;
    bbox["h"] = r.bbox_h;
    rock["bbox"] = std::move(bbox);

    return rock;

}

// buildDetectResponse()
crow::json::wvalue Server::buildDetectResponse(

    const DetectionResult&  result,
    const MetrologyReport&  report,
    const std::string&      job_id,
    const std::string&      conveyor_id,
    const std::string&      location_code) const
{
    crow::json::wvalue body;

-
    // Job metadata

    body["job_id"]        = job_id;
    body["conveyor_id"]   = conveyor_id;
    body["location_code"] = location_code;
    body["rock_count"]    = result.rock_count;


    // Individual rock detections

    std::vector<crow::json::wvalue> detections;

    detections.reserve(result.rocks.size());

    for (const auto& r : result.rocks) {

        detections.push_back(rockToJson(r));
    }
    body["detections"] = std::move(detections);


    // Job-level aggregates

    crow::json::wvalue aggregates;

    aggregates["total_volume_cm3"]  = report.total_volume_cm3;
    aggregates["total_mass_g"]      = report.total_mass_g;
    aggregates["mean_equiv_diam"]   = report.mean_equiv_diam;
    aggregates["mean_sphericity"]   = report.mean_sphericity;
    aggregates["mean_aspect_ratio"] = report.mean_aspect_ratio;
    body["aggregates"] = std::move(aggregates);

    // Granulometry curve

    crow::json::wvalue gran;
    gran["D30"] = report.granulometry.D30;
    gran["D50"] = report.granulometry.D50;
    gran["D80"] = report.granulometry.D80;

    // Curve points for Recharts LineChart
    std::vector<crow::json::wvalue> curve_pts;

    curve_pts.reserve(report.granulometry.diameters_cm.size());

    for (std::size_t i = 0;

         i < report.granulometry.diameters_cm.size(); ++i)
    {
        crow::json::wvalue pt;

        pt["d"]  = report.granulometry.diameters_cm[i];

        pt["cp"] = report.granulometry.cumulative_passing[i];

        curve_pts.push_back(std::move(pt));
    }

    gran["curve"] = std::move(curve_pts);
    body["granulometry"] = std::move(gran);


    // Histograms

    auto serializeHistogram =
        [](const Histogram& h) -> crow::json::wvalue
    {
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

    histograms["equiv_diameter"] =
        serializeHistogram(report.hist_equiv_diameter);

    histograms["volume"]         =
        serializeHistogram(report.hist_volume);

    histograms["sphericity"]     =
        serializeHistogram(report.hist_sphericity);

    histograms["aspect_ratio"]   =
        serializeHistogram(report.hist_aspect_ratio);

    body["histograms"] = std::move(histograms);


    // Clustering

    crow::json::wvalue clustering;
    clustering["num_clusters"]     =
        report.clustering.num_clusters;
    clustering["compactness"]      =
        report.clustering.compactness;
    clustering["pca_variance_pc1"] =
        report.clustering.pca_variance_pc1;
    clustering["pca_variance_pc2"] =
        report.clustering.pca_variance_pc2;

    // Per-rock PCA coordinates and cluster label
    std::vector<crow::json::wvalue> scatter;

    scatter.reserve(result.rocks.size());

    for (std::size_t i = 0; i < result.rocks.size(); ++i) {

        crow::json::wvalue pt;

        pt["rock_id"] = result.rocks[i].rock_id;
        pt["x"]   = report.clustering.pca_x[i];
        pt["y"]   = report.clustering.pca_y[i];
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

    // Cluster centroids for scatter plot overlay
    std::vector<crow::json::wvalue> centroids;
    centroids.reserve(
        static_cast<std::size_t>(report.clustering.num_clusters)
    );
    for (int k = 0; k < report.clustering.num_clusters; ++k) {

        crow::json::wvalue c;

        c["cluster"] = k;
        c["x"]       = report.clustering.centroid_pca_x[k];
        c["y"]       = report.clustering.centroid_pca_y[k];

        if (k < static_cast<int>(

                report.clustering.cluster_names.size()))
        {
            c["name"] = report.clustering.cluster_names[k];
        }
        centroids.push_back(std::move(c));

    }
    clustering["centroids"] = std::move(centroids);

    body["clustering"] = std::move(clustering);


    return body;

}

// Simple UUID v4 generator using random_device.
std::string Server::generateUUID() const
{
    static thread_local std::mt19937 rng(

        std::random_device{}()
    );
    
    static thread_local std::uniform_int_distribution<int> dist(0, 15);
    static thread_local std::uniform_int_distribution<int>dist2(8, 11);

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

// End rockpulse namespace 

}