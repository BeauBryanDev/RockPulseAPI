#pragma once

#include <crow.h>
#include <memory>
#include <string>

#include "inference/rock_detector.hpp"
#include "inference/metrology.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {

struct ServerConfig {
    int         port        = 8080;
    int         concurrency = 4;
    bool        enable_cors = true;
    std::string outputs_dir = "data/outputs";
    std::string inputs_dir  = "data/inputs";
    std::string log_level   = "info";
};

struct AuthMiddleware : public crow::ILocalMiddleware {

    struct context {
        TokenValidation token_validation;
    };

    void before_handle(
        crow::request&  req,
        crow::response& res,
        context&        ctx
    );

    void after_handle(
        crow::request&  req,
        crow::response& res,
        context&        ctx
    );

    std::shared_ptr<PostgresConnector> db;
};

struct CorsMiddleware : public crow::ILocalMiddleware {

    struct context {};

    void before_handle(
        crow::request&  req,
        crow::response& res,
        context&        ctx
    );

    void after_handle(
        crow::request&  req,
        crow::response& res,
        context&        ctx
    );
};

using RockPulseApp = crow::App<CorsMiddleware, AuthMiddleware>;

class Server {
public:

    Server(
        const ServerConfig&   server_cfg,
        const DBConfig&       db_cfg,
        const DetectorConfig& detector_cfg
    );

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    void run();
    bool isReady() const;

private:

    void registerRoutes();

    ServerConfig                        server_cfg_;
    std::shared_ptr<RockDetector>       detector_;
    std::shared_ptr<PostgresConnector>  db_;
    RockPulseApp                        app_;
};

} // namespace RockPulse
