#include "api/handlers/conveyors_handler.hpp"
#include <optional>

namespace RockPulse {
namespace Handlers {

namespace {

crow::json::wvalue serializeConveyor(const pqxx::row& row)
{
    crow::json::wvalue conv;
    conv["conveyor_id"]   = row["conveyor_id"].as<std::string>();
    conv["location_id"]   = row["location_id"].as<std::string>();
    conv["conveyor_code"] = row["conveyor_code"].as<std::string>();

    if (row["base_rock_density"].is_null())
        conv["base_rock_density"] = nullptr;
    else
        conv["base_rock_density"] = row["base_rock_density"].as<double>();

    if (row["calibration_marker_cm"].is_null())
        conv["calibration_marker_cm"] = nullptr;
    else
        conv["calibration_marker_cm"] = row["calibration_marker_cm"].as<double>();

    if (row["image_rate_minutes"].is_null())
        conv["image_rate_minutes"] = nullptr;
    else
        conv["image_rate_minutes"] = row["image_rate_minutes"].as<int>();

    if (row["note_description"].is_null())
        conv["note_description"] = nullptr;
    else
        conv["note_description"] = row["note_description"].as<std::string>();

    conv["created_at"] = row["created_at"].as<std::string>();
    return conv;
}

} // anonymous namespace

// POST /api/v1/conveyors
crow::response createConveyor(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    const auto body = crow::json::load(req.body);
    if (!body) {
        crow::json::wvalue err;
        err["error"]  = "Invalid JSON body.";
        err["detail"] = "Content-Type must be application/json.";
        return crow::response(400, err);
    }

    if (!body.has("conveyor_code")) {
        crow::json::wvalue err;
        err["error"] = "Missing required field: conveyor_code.";
        return crow::response(400, err);
    }

    const std::string code = std::string(body["conveyor_code"].s());
    if (code.empty()) {
        crow::json::wvalue err;
        err["error"] = "conveyor_code must not be empty.";
        return crow::response(422, err);
    }

    const std::string& location_id = auth_ctx.token_validation.location_id;

    std::optional<float> base_density;
    std::optional<float> marker_cm;
    std::optional<int>   image_rate;
    std::string          note;

    if (body.has("base_rock_density"))     base_density = static_cast<float>(body["base_rock_density"].d());
    if (body.has("calibration_marker_cm")) marker_cm    = static_cast<float>(body["calibration_marker_cm"].d());
    if (body.has("image_rate_minutes"))    image_rate   = static_cast<int>(body["image_rate_minutes"].d());
    if (body.has("note_description"))      note         = std::string(body["note_description"].s());

    try {
        const std::string conveyor_id = db.insertConveyor(
            location_id, code, base_density, marker_cm, image_rate, note
        );

        crow::json::wvalue res_body;
        res_body["conveyor_id"]   = conveyor_id;
        res_body["location_id"]   = location_id;
        res_body["conveyor_code"] = code;

        crow::response res(201, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to create conveyor.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// GET /api/v1/conveyors
crow::response listConveyors(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    const std::string& location_id = auth_ctx.token_validation.location_id;

    try {
        pqxx::result rows = db.listConveyorsByLocation(location_id);

        std::vector<crow::json::wvalue> conveyors;
        conveyors.reserve(rows.size());
        for (const auto& row : rows)
            conveyors.push_back(serializeConveyor(row));

        crow::json::wvalue body;
        body["conveyors"]   = std::move(conveyors);
        body["location_id"] = location_id;

        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to list conveyors.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// GET /api/v1/conveyors/<id>
crow::response getConveyor(
    const crow::request&     req,
    const std::string&       conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    try {
        pqxx::result rows = db.getConveyorFull(conveyor_id);

        if (rows.empty()) {
            crow::json::wvalue err;
            err["error"] = "Conveyor not found.";
            return crow::response(404, err);
        }

        if (rows[0]["location_id"].as<std::string>() != auth_ctx.token_validation.location_id) {
            crow::json::wvalue err;
            err["error"] = "Forbidden: conveyor does not belong to your location.";
            return crow::response(403, err);
        }

        crow::json::wvalue body = serializeConveyor(rows[0]);
        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to get conveyor.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// PUT /api/v1/conveyors/<id>
crow::response updateConveyor(
    const crow::request&     req,
    const std::string&       conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    const auto body = crow::json::load(req.body);
    if (!body) {
        crow::json::wvalue err;
        err["error"]  = "Invalid JSON body.";
        err["detail"] = "Content-Type must be application/json.";
        return crow::response(400, err);
    }

    if (!body.has("conveyor_code")) {
        crow::json::wvalue err;
        err["error"] = "Missing required field: conveyor_code.";
        return crow::response(400, err);
    }

    const std::string code = std::string(body["conveyor_code"].s());
    if (code.empty()) {
        crow::json::wvalue err;
        err["error"] = "conveyor_code must not be empty.";
        return crow::response(422, err);
    }

    try {
        pqxx::result existing = db.getConveyorFull(conveyor_id);
        if (existing.empty()) {
            crow::json::wvalue err;
            err["error"] = "Conveyor not found.";
            return crow::response(404, err);
        }
        if (existing[0]["location_id"].as<std::string>() != auth_ctx.token_validation.location_id) {
            crow::json::wvalue err;
            err["error"] = "Forbidden: conveyor does not belong to your location.";
            return crow::response(403, err);
        }

        std::optional<float> base_density;
        std::optional<float> marker_cm;
        std::optional<int>   image_rate;
        std::string          note;

        if (body.has("base_rock_density"))     base_density = static_cast<float>(body["base_rock_density"].d());
        if (body.has("calibration_marker_cm")) marker_cm    = static_cast<float>(body["calibration_marker_cm"].d());
        if (body.has("image_rate_minutes"))    image_rate   = static_cast<int>(body["image_rate_minutes"].d());
        if (body.has("note_description"))      note         = std::string(body["note_description"].s());

        db.updateConveyor(conveyor_id, code, base_density, marker_cm, image_rate, note);

        pqxx::result rows = db.getConveyorFull(conveyor_id);
        crow::json::wvalue res_body = serializeConveyor(rows[0]);
        crow::response res(200, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        if (msg.find("not found") != std::string::npos) {
            crow::json::wvalue err;
            err["error"] = "Conveyor not found.";
            return crow::response(404, err);
        }
        crow::json::wvalue err;
        err["error"]  = "Failed to update conveyor.";
        err["detail"] = msg;
        return crow::response(500, err);
    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to update conveyor.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// PATCH /api/v1/conveyors/<id>  — calibration_marker_cm only
crow::response patchConveyor(
    const crow::request&     req,
    const std::string&       conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
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

// DELETE /api/v1/conveyors/<id>
crow::response deleteConveyor(
    const crow::request&     req,
    const std::string&       conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    try {
        pqxx::result existing = db.getConveyorFull(conveyor_id);
        if (existing.empty()) {
            crow::json::wvalue err;
            err["error"] = "Conveyor not found.";
            return crow::response(404, err);
        }
        if (existing[0]["location_id"].as<std::string>() != auth_ctx.token_validation.location_id) {
            crow::json::wvalue err;
            err["error"] = "Forbidden: conveyor does not belong to your location.";
            return crow::response(403, err);
        }

        db.deleteConveyor(conveyor_id);

        crow::json::wvalue res_body;
        res_body["message"]     = "Conveyor deleted.";
        res_body["conveyor_id"] = conveyor_id;

        crow::response res(200, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        if (msg.find("not found") != std::string::npos) {
            crow::json::wvalue err;
            err["error"] = "Conveyor not found.";
            return crow::response(404, err);
        }
        crow::json::wvalue err;
        err["error"]  = "Failed to delete conveyor.";
        err["detail"] = msg;
        return crow::response(500, err);
    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to delete conveyor.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

} // namespace Handlers
} // namespace RockPulse
