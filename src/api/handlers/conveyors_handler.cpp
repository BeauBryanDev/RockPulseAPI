#include "api/handlers/conveyors_handler.hpp"
#include <crow/json.h>
#include <iostream>

namespace RockPulse {
namespace Handlers {

crow::response patchConveyor(
    const crow::request&        req,
    const std::string&          conveyor_id,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db)
{
    const auto body = crow::json::load(req.body);

    if (!body) {
        crow::json::wvalue err;
        err["error"] = "Invalid JSON body.";
        return crow::response(400, err);
    }

    if (!body.has("calibration_marker_cm")) {
        crow::json::wvalue err;
        err["error"] = "Missing required field: calibration_marker_cm.";
        return crow::response(400, err);
    }

    const float marker_cm = static_cast<float>(body["calibration_marker_cm"].d());

    if (marker_cm <= 0.0f) {
        crow::json::wvalue err;
        err["error"] = "calibration_marker_cm must be a positive value (cm).";
        return crow::response(422, err);
    }

    try {
        const ConveyorRow conveyor = db.getConveyorById(conveyor_id);

        // Token's location must own this conveyor.
        if (conveyor.location_id != auth_ctx.token_validation.location_id) {
            crow::json::wvalue err;
            err["error"] = "Forbidden: conveyor does not belong to your location.";
            return crow::response(403, err);
        }

        db.updateConveyorMarker(conveyor_id, marker_cm);

    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();

        if (msg.find("not found") != std::string::npos) {
            crow::json::wvalue err;
            err["error"] = "Conveyor not found.";
            return crow::response(404, err);
        }

        crow::json::wvalue err;
        err["error"]  = "Database error.";
        err["detail"] = msg;
        return crow::response(500, err);
    }

    crow::json::wvalue res_body;
    res_body["conveyor_id"]           = conveyor_id;
    res_body["calibration_marker_cm"] = static_cast<double>(marker_cm);

    crow::response res(200, res_body);
    res.set_header("Content-Type", "application/json");
    return res;
}

} // namespace Handlers
} // namespace RockPulse
