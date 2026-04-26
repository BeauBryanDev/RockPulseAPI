#include "api/handlers/tokens_handler.hpp"
#include <random>
#include <sstream>

namespace RockPulse {
namespace Handlers {

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

} // anonymous namespace

// POST /api/v1/tokens
crow::response createToken(
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

    if (!body.has("owner_name")) {
        crow::json::wvalue err;
        err["error"] = "Missing required field: owner_name.";
        return crow::response(400, err);
    }

    const std::string owner_name = std::string(body["owner_name"].s());
    if (owner_name.empty()) {
        crow::json::wvalue err;
        err["error"] = "owner_name must not be empty.";
        return crow::response(422, err);
    }

    const std::string& location_id = auth_ctx.token_validation.location_id;

    // Two UUID4s concatenated → 72-char raw token. Never stored.
    const std::string raw_token = generateUUID() + generateUUID();

    try {
        db.insertToken(location_id, owner_name, raw_token);

        crow::json::wvalue res_body;
        res_body["token"]       = raw_token;
        res_body["owner_name"]  = owner_name;
        res_body["location_id"] = location_id;
        res_body["warning"]     = "Save this token now. It will never be shown again.";

        crow::response res(201, res_body);
        res.set_header("Content-Type", "application/json");
        return res;

    } catch (const std::exception& e) {
        crow::json::wvalue err;
        err["error"]  = "Failed to create token.";
        err["detail"] = e.what();
        return crow::response(500, err);
    }
}

} // namespace Handlers
} // namespace RockPulse
