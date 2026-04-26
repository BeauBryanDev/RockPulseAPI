#include "api/handlers/location_handler.hpp"
#include <cmath>
#include <pqxx/pqxx>

namespace RockPulse {
namespace Handlers {

namespace {

crow::json::wvalue serializeLocation(const pqxx::row& row)
{
    crow::json::wvalue loc;
    loc["location_id"]   = row["location_id"].as<std::string>();
    loc["location_code"] = row["location_code"].as<std::string>();
    loc["location_name"] = row["location_name"].as<std::string>();

    if (row["location_description"].is_null())
        loc["location_description"] = nullptr;
    else
        loc["location_description"] = row["location_description"].as<std::string>();

    if (row["avg_rock_density"].is_null())
        loc["avg_rock_density"] = nullptr;
    else
        loc["avg_rock_density"] = row["avg_rock_density"].as<double>();

    loc["fi_alpha"] = row["fi_alpha"].as<double>();
    loc["fi_beta"]  = row["fi_beta"].as<double>();
    loc["fi_gamma"] = row["fi_gamma"].as<double>();

    if (row["location_latitude"].is_null())
        loc["location_latitude"] = nullptr;
    else
        loc["location_latitude"] = row["location_latitude"].as<double>();

    if (row["location_longitude"].is_null())
        loc["location_longitude"] = nullptr;
    else
        loc["location_longitude"] = row["location_longitude"].as<double>();

    loc["created_at"] = row["created_at"].as<std::string>();
    return loc;
}

void parseLocationFields(
    const crow::json::rvalue& body,
    std::string&          name,
    std::string&          description,
    std::optional<float>& avg_density,
    float&                fi_alpha,
    float&                fi_beta,
    float&                fi_gamma,
    std::optional<float>& latitude,
    std::optional<float>& longitude)
{
    if (body.has("location_name"))        name        = std::string(body["location_name"].s());
    if (body.has("location_description")) description = std::string(body["location_description"].s());
    if (body.has("avg_rock_density"))     avg_density = static_cast<float>(body["avg_rock_density"].d());
    if (body.has("fi_alpha"))             fi_alpha    = static_cast<float>(body["fi_alpha"].d());
    if (body.has("fi_beta"))              fi_beta     = static_cast<float>(body["fi_beta"].d());
    if (body.has("fi_gamma"))             fi_gamma    = static_cast<float>(body["fi_gamma"].d());
    if (body.has("location_latitude"))    latitude    = static_cast<float>(body["location_latitude"].d());
    if (body.has("location_longitude"))   longitude   = static_cast<float>(body["location_longitude"].d());
}

} // anonymous namespace

// POST /api/v1/locations
crow::response createLocation(
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

    if (!body.has("location_code") || !body.has("location_name")) {
        crow::json::wvalue err;
        err["error"] = "Missing required fields: location_code, location_name.";
        return crow::response(400, err);
    }

    const std::string code = std::string(body["location_code"].s());
    const std::string name = std::string(body["location_name"].s());

    if (code.empty() || name.empty()) {
        crow::json::wvalue err;
        err["error"] = "location_code and location_name must not be empty.";
        return crow::response(422, err);
    }

    std::string          name_copy = name;
    std::string          description;
    std::optional<float> avg_density;
    float                fi_alpha = 0.33f;
    float                fi_beta  = 0.33f;
    float                fi_gamma = 0.34f;
    std::optional<float> latitude;
    std::optional<float> longitude;

    parseLocationFields(body, name_copy, description, avg_density,
                        fi_alpha, fi_beta, fi_gamma, latitude, longitude);

    try {
        const std::string location_id = db.insertLocation(
            code, name, description,
            avg_density, fi_alpha, fi_beta, fi_gamma,
            latitude, longitude
        );

        crow::json::wvalue res_body;
        res_body["location_id"]   = location_id;
        res_body["location_code"] = code;
        res_body["location_name"] = name;

        crow::response res(201, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const pqxx::sql_error& e) {
        if (std::string(e.sqlstate()) == "23505") {
            crow::json::wvalue err;
            err["error"]  = "location_code already exists.";
            err["detail"] = e.what();
            return crow::response(409, err);
        }
        crow::json::wvalue err;
        err["error"]  = "Database error.";
        err["detail"] = e.what();
        return crow::response(500, err);
    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to create location.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// GET /api/v1/locations
crow::response listLocations(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    const std::string& location_id = auth_ctx.token_validation.location_id;

    try {
        pqxx::result rows = db.getLocationById(location_id);

        std::vector<crow::json::wvalue> locations;
        locations.reserve(rows.size());
        for (const auto& row : rows)
            locations.push_back(serializeLocation(row));

        crow::json::wvalue body;
        body["locations"] = std::move(locations);

        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to list locations.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// GET /api/v1/locations/<id>
crow::response getLocation(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    if (auth_ctx.token_validation.location_id != location_id) {
        crow::json::wvalue err;
        err["error"] = "Forbidden: location does not belong to your token.";
        return crow::response(403, err);
    }

    try {
        pqxx::result rows = db.getLocationById(location_id);

        if (rows.empty()) {
            crow::json::wvalue err;
            err["error"] = "Location not found.";
            return crow::response(404, err);
        }

        crow::json::wvalue body = serializeLocation(rows[0]);
        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to get location.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// PUT /api/v1/locations/<id>
crow::response updateLocation(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    if (auth_ctx.token_validation.location_id != location_id) {
        crow::json::wvalue err;
        err["error"] = "Forbidden: location does not belong to your token.";
        return crow::response(403, err);
    }

    const auto body = crow::json::load(req.body);
    if (!body) {
        crow::json::wvalue err;
        err["error"]  = "Invalid JSON body.";
        err["detail"] = "Content-Type must be application/json.";
        return crow::response(400, err);
    }

    if (!body.has("location_name")) {
        crow::json::wvalue err;
        err["error"] = "Missing required field: location_name.";
        return crow::response(400, err);
    }

    std::string          name;
    std::string          description;
    std::optional<float> avg_density;
    float                fi_alpha = 0.33f;
    float                fi_beta  = 0.33f;
    float                fi_gamma = 0.34f;
    std::optional<float> latitude;
    std::optional<float> longitude;

    parseLocationFields(body, name, description, avg_density,
                        fi_alpha, fi_beta, fi_gamma, latitude, longitude);

    if (name.empty()) {
        crow::json::wvalue err;
        err["error"] = "location_name must not be empty.";
        return crow::response(422, err);
    }

    try {
        db.updateLocation(location_id, name, description,
                          avg_density, fi_alpha, fi_beta, fi_gamma,
                          latitude, longitude);

        pqxx::result rows = db.getLocationById(location_id);
        crow::json::wvalue res_body = serializeLocation(rows[0]);
        crow::response res(200, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        if (msg.find("not found") != std::string::npos) {
            crow::json::wvalue err;
            err["error"] = "Location not found.";
            return crow::response(404, err);
        }
        crow::json::wvalue err;
        err["error"]  = "Failed to update location.";
        err["detail"] = msg;
        return crow::response(500, err);
    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to update location.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// PATCH /api/v1/locations/<id>  — FI calibration weights only
crow::response patchLocationFI(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    if (auth_ctx.token_validation.location_id != location_id) {
        crow::json::wvalue err;
        err["error"] = "Forbidden: location does not belong to your token.";
        return crow::response(403, err);
    }

    const auto body = crow::json::load(req.body);
    if (!body) {
        crow::json::wvalue err;
        err["error"]  = "Invalid JSON body.";
        err["detail"] = "Content-Type must be application/json.";
        return crow::response(400, err);
    }

    if (!body.has("fi_alpha") || !body.has("fi_beta") || !body.has("fi_gamma")) {
        crow::json::wvalue err;
        err["error"] = "Missing required fields: fi_alpha, fi_beta, fi_gamma.";
        return crow::response(400, err);
    }

    const float alpha = static_cast<float>(body["fi_alpha"].d());
    const float beta  = static_cast<float>(body["fi_beta"].d());
    const float gamma = static_cast<float>(body["fi_gamma"].d());

    if (alpha < 0.0f || beta < 0.0f || gamma < 0.0f) {
        crow::json::wvalue err;
        err["error"] = "fi_alpha, fi_beta, fi_gamma must all be >= 0.";
        return crow::response(422, err);
    }

    const float sum = alpha + beta + gamma;
    if (std::abs(sum - 1.0f) > 0.01f) {
        crow::json::wvalue err;
        err["error"]  = "fi_alpha + fi_beta + fi_gamma must sum to 1.0 (tolerance ±0.01).";
        err["detail"] = "Current sum: " + std::to_string(sum);
        return crow::response(422, err);
    }

    try {
        db.patchLocationFIParams(location_id, alpha, beta, gamma);

        crow::json::wvalue res_body;
        res_body["location_id"] = location_id;
        res_body["fi_alpha"]    = static_cast<double>(alpha);
        res_body["fi_beta"]     = static_cast<double>(beta);
        res_body["fi_gamma"]    = static_cast<double>(gamma);

        crow::response res(200, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        if (msg.find("not found") != std::string::npos) {
            crow::json::wvalue err;
            err["error"] = "Location not found.";
            return crow::response(404, err);
        }
        crow::json::wvalue err;
        err["error"]  = "Failed to update FI parameters.";
        err["detail"] = msg;
        return crow::response(500, err);
    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to update FI parameters.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

// DELETE /api/v1/locations/<id>
crow::response deleteLocation(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db)
{
    if (auth_ctx.token_validation.location_id != location_id) {
        crow::json::wvalue err;
        err["error"] = "Forbidden: location does not belong to your token.";
        return crow::response(403, err);
    }

    try {
        db.deleteLocation(location_id);

        crow::json::wvalue res_body;
        res_body["message"]     = "Location deleted.";
        res_body["location_id"] = location_id;

        crow::response res(200, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        if (msg.find("not found") != std::string::npos) {
            crow::json::wvalue err;
            err["error"] = "Location not found.";
            return crow::response(404, err);
        }
        crow::json::wvalue err;
        err["error"]  = "Failed to delete location.";
        err["detail"] = msg;
        return crow::response(500, err);
    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to delete location.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

} // namespace Handlers
} // namespace RockPulse
