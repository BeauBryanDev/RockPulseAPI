#include "api/handlers/detections_handler.hpp"

#include <algorithm>
#include <stdexcept>

namespace RockPulse {
namespace Handlers {

crow::response listDetections(
    const crow::request&        req,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db)
{
    const std::string& conveyor_id =
        auth_ctx.token_validation.conveyor.conveyor_id;

    int limit  = 20;
    int offset = 0;

    const auto limit_param  = req.url_params.get("limit");
    const auto offset_param = req.url_params.get("offset");

    if (limit_param)  limit  = std::stoi(limit_param);
    if (offset_param) offset = std::stoi(offset_param);

    limit  = std::max(1,  std::min(limit,  100));
    offset = std::max(0,  offset);

    try {
        pqxx::result rows = db.listDetectionJobs(conveyor_id, limit, offset);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> jobs;
        jobs.reserve(rows.size());

        for (const auto& row : rows) {
            crow::json::wvalue job;
            job["job_id"]      = row["job_id"].as<std::string>();
            job["created_at"]  = row["created_at"].as<std::string>();
            job["rock_count"]  = row["rock_count"].as<int>();
            job["image_path"]  = row["image_path"].as<std::string>();
            job["output_path"] = row["output_path"].as<std::string>();
            job["density"]     = row["density"].as<double>();
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

crow::response getDetection(
    const crow::request&        req,
    const std::string&          job_id,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db)
{
    if (job_id.empty()) {
        crow::json::wvalue err;
        err["error"] = "job_id is required.";
        return crow::response(400, err);
    }

    try {
        pqxx::result rows = db.getDetectionJob(job_id);

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

            crow::json::wvalue bbox;
            bbox["x"] = row["bbox_x"].as<int>();
            bbox["y"] = row["bbox_y"].as<int>();
            bbox["w"] = row["bbox_w"].as<int>();
            bbox["h"] = row["bbox_h"].as<int>();
            rock["bbox"] = std::move(bbox);

            if (!row["fragment_index"].is_null()) {
                rock["fragment_index"] = row["fragment_index"].as<double>();
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

} // namespace Handlers
} // namespace RockPulse
