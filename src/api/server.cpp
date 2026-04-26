#include "api/server.hpp"
#include "api/handlers/conveyors_handler.hpp"
#include "api/handlers/detect_handler.hpp"
#include "api/handlers/detections_handler.hpp"
#include "api/handlers/granulometry_handler.hpp"
#include "api/handlers/health_handler.hpp"
#include "api/handlers/location_handler.hpp"
#include "api/handlers/tokens_handler.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace RockPulse {

// CorsMiddleware

void CorsMiddleware::before_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx)
{
    if (req.method == crow::HTTPMethod::Options) {

        res.code = 204;

        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers",
                       "Content-Type, X-API-KEY, Authorization");
        res.add_header("Access-Control-Max-Age", "86400");
        
        res.end();
    }
}

void CorsMiddleware::after_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx)
{
    res.add_header("Access-Control-Allow-Origin",  "*");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers",
                   "Content-Type, X-API-KEY, Authorization");
}

// AuthMiddleware

void AuthMiddleware::before_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx)
{
    if (req.url == "/api/health") return;

    // Reject oversized requests before inference or DB work.
    // Check Content-Length header first (fast path for well-behaved clients).
    static constexpr std::size_t kMaxBodyBytes = 20ULL * 1024 * 1024;
    const std::string cl_header = req.get_header_value("Content-Length");
    if (!cl_header.empty()) {
        try {
            if (std::stoll(cl_header) > static_cast<long long>(kMaxBodyBytes)) {
                res.code = 413;
                res.write(R"({"error":"Request body exceeds 20 MB limit.","detail":"Maximum allowed size is 20971520 bytes."})");
                res.end();
                return;
            }
        } catch (...) {}
    }
    if (req.body.size() > kMaxBodyBytes) {
        res.code = 413;
        res.write(R"({"error":"Request body exceeds 20 MB limit.","detail":"Maximum allowed size is 20971520 bytes."})");
        res.end();
        return;
    }

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
        res.write(R"({"error":")" + ctx.token_validation.error_msg + R"("})");
        res.end();
    }
}

void AuthMiddleware::after_handle(
    crow::request&  req,
    crow::response& res,
    context&        ctx) {}

// Server

Server::Server(
    const ServerConfig&   server_cfg,
    const DBConfig&       db_cfg,
    const DetectorConfig& detector_cfg)
    : server_cfg_(server_cfg)
{
    db_       = std::make_shared<PostgresConnector>(db_cfg);
    detector_ = std::make_shared<RockDetector>(detector_cfg);

    app_.get_middleware<AuthMiddleware>().db = db_;

    fs::create_directories(server_cfg_.outputs_dir);
    fs::create_directories(server_cfg_.inputs_dir);

    registerRoutes();

    std::cout << "[Server] RockPulseAPI initialized.\n"
              << "[Server] Detector ready : "
              << (detector_->isReady() ? "YES" : "NO") << "\n"
              << "[Server] DB ready       : "
              << (db_->isReady()       ? "YES" : "NO") << "\n";
}

bool Server::isReady() const
{
    return detector_->isReady() && db_->isReady();
}

void Server::run()
{
    std::cout << "[Server] Listening on port " << server_cfg_.port << "\n";
    app_.port(server_cfg_.port)
        .concurrency(server_cfg_.concurrency)
        .run();
}

void Server::registerRoutes()
{

    // Public

    CROW_ROUTE(app_, "/api/health")
        .methods(crow::HTTPMethod::Get)
        ([this](const crow::request& req) {
            return Handlers::health(req, *detector_, *db_);
        });

    // Inference

    CROW_ROUTE(app_, "/api/v1/detect")
        .methods(crow::HTTPMethod::Post)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            res = Handlers::detect(req, auth_ctx, *detector_, *db_, server_cfg_);
            res.end();
        });


    // Detections (read)

    CROW_ROUTE(app_, "/api/v1/detections")
        .methods(crow::HTTPMethod::Get)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            res = Handlers::listDetections(req, auth_ctx, *db_);
            res.end();
        });

    CROW_ROUTE(app_, "/api/v1/detections/<string>")
        .methods(crow::HTTPMethod::Get)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res,
                const std::string& job_id) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            res = Handlers::getDetection(req, job_id, auth_ctx, *db_);
            res.end();
        });


    // Analytics

    CROW_ROUTE(app_, "/api/v1/analytics/granulometry/<string>")
        .methods(crow::HTTPMethod::Get)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res,
                const std::string& job_id) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            res = Handlers::granulometry(req, job_id, auth_ctx, *db_);
            res.end();
        });


    // Locations (full CRUD)

    CROW_ROUTE(app_, "/api/v1/locations")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            if (req.method == crow::HTTPMethod::Post)
                res = Handlers::createLocation(req, auth_ctx, *db_);
            else
                res = Handlers::listLocations(req, auth_ctx, *db_);
            res.end();
        });

    CROW_ROUTE(app_, "/api/v1/locations/<string>")
        .methods(crow::HTTPMethod::Get,
                 crow::HTTPMethod::Put,
                 crow::HTTPMethod::Patch,
                 crow::HTTPMethod::Delete)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res,
                const std::string& loc_id) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            if (req.method == crow::HTTPMethod::Get)
                res = Handlers::getLocation(req, loc_id, auth_ctx, *db_);
            else if (req.method == crow::HTTPMethod::Put)
                res = Handlers::updateLocation(req, loc_id, auth_ctx, *db_);
            else if (req.method == crow::HTTPMethod::Patch)
                res = Handlers::patchLocationFI(req, loc_id, auth_ctx, *db_);
            else
                res = Handlers::deleteLocation(req, loc_id, auth_ctx, *db_);
            res.end();
        });


    // Conveyors (full CRUD)

    CROW_ROUTE(app_, "/api/v1/conveyors")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            if (req.method == crow::HTTPMethod::Post)
                res = Handlers::createConveyor(req, auth_ctx, *db_);
            else
                res = Handlers::listConveyors(req, auth_ctx, *db_);
            res.end();
        });

    CROW_ROUTE(app_, "/api/v1/conveyors/<string>")
        .methods(crow::HTTPMethod::Get,
                 crow::HTTPMethod::Put,
                 crow::HTTPMethod::Patch,
                 crow::HTTPMethod::Delete)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res,
                const std::string& conv_id) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            if (req.method == crow::HTTPMethod::Get)
                res = Handlers::getConveyor(req, conv_id, auth_ctx, *db_);
            else if (req.method == crow::HTTPMethod::Put)
                res = Handlers::updateConveyor(req, conv_id, auth_ctx, *db_);
            else if (req.method == crow::HTTPMethod::Patch)
                res = Handlers::patchConveyor(req, conv_id, auth_ctx, *db_);
            else
                res = Handlers::deleteConveyor(req, conv_id, auth_ctx, *db_);
            res.end();
        });


    // Tokens

    CROW_ROUTE(app_, "/api/v1/tokens")
        .methods(crow::HTTPMethod::Post)
        .CROW_MIDDLEWARES(app_, CorsMiddleware, AuthMiddleware)
        ([this](const crow::request& req, crow::response& res) {
            auto& auth_ctx = app_.get_context<AuthMiddleware>(req);
            res = Handlers::createToken(req, auth_ctx, *db_);
            res.end();
        });

    CROW_ROUTE(app_, "/outputs/<string>")
        .methods(crow::HTTPMethod::Get)
        ([this](const crow::request& req, const std::string& filename) {
            const std::string path = server_cfg_.outputs_dir + "/" + filename;
            if (!fs::exists(path)) return crow::response(404);
            std::ifstream f(path, std::ios::binary);
            std::string body((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
            crow::response res(200, body);
            res.set_header("Content-Type", "image/png");
            return res;
        });
}

} // namespace RockPulse
