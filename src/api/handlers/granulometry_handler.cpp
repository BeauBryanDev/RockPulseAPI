#include "api/handlers/granulometry_handler.hpp"
#include "inference/metrology.hpp"
#include "inference/metrics_calculator.hpp"

namespace RockPulse {
namespace Handlers {

crow::response granulometry(
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

    const std::string& conveyor_id =
        auth_ctx.token_validation.conveyor.conveyor_id;

    try {
        // Query returns only the two fields computeGranulometry needs.
        // The JOIN on detection_jobs.conveyor_id enforces ownership:
        // a token from conveyor A cannot read jobs from conveyor B.
        pqxx::result rows = db.getGranulometryData(job_id, conveyor_id);

        if (rows.empty()) {
            crow::json::wvalue err;
            err["error"] = "No detections found for job: " + job_id;
            return crow::response(404, err);
        }

        // Build minimal RockMetrics — computeGranulometry only touches
        // equiv_diameter and volume_cm3, all other fields stay at default.
        std::vector<RockMetrics> rocks;
        rocks.reserve(rows.size());

        for (const auto& row : rows) {
            RockMetrics r;
            r.equiv_diameter = row["equiv_diameter"].as<double>();
            r.volume_cm3     = row["volume_cm3"].as<float>();
            rocks.push_back(r);
        }

        GranulometryCurve curve = MetrologyEngine::computeGranulometry(rocks);

        crow::json::wvalue body;
        body["job_id"]     = job_id;
        body["rock_count"] = static_cast<int>(rocks.size());
        body["D30"]        = curve.D30;
        body["D50"]        = curve.D50;
        body["D80"]        = curve.D80;

        // curve array — one point per rock, sorted ascending by diameter.
        // Shape: [{ d: number, cp: number }, ...]
        // d  = equivalent diameter in cm (x-axis)
        // cp = cumulative volume fraction [0.0–1.0] (y-axis)
        std::vector<crow::json::wvalue> curve_pts;
        curve_pts.reserve(curve.diameters_cm.size());

        for (std::size_t i = 0; i < curve.diameters_cm.size(); ++i) {
            crow::json::wvalue pt;
            pt["d"]  = curve.diameters_cm[i];
            pt["cp"] = curve.cumulative_passing[i];
            curve_pts.push_back(std::move(pt));
        }
        body["curve"] = std::move(curve_pts);

        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to compute granulometry.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

} // namespace Handlers
} // namespace RockPulse
