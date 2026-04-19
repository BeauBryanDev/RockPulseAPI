#include <crow.h>
#include <memory>
#include <string>
#include <random>
#include <sstream>

#include "inference/rock_detector.hpp"
#include "inference/metrology.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {

struct ServerConfig {
    int         port          = 8080;
    int         concurrency   = 4;      /
    bool        enable_cors   = true;
    std::string outputs_dir   = "data/outputs";
    std::string inputs_dir    = "data/inputs";
    std::string log_level     = "info";
};

// Crow middleware - validates X-API-KEY on every protected
// route before the handler executes.
// Injects TokenValidation into crow::context so handlers
// can read location and conveyor context without a second DB trip .
struct AuthMiddleware {

    struct context {
        TokenValidation token_validation;
    };

    // before_handle is called before the route handler.
    // Routes tagged CROW_MIDDLEWARE(AuthMiddleware) go through here.
    void before_handle(
        crow::request&  req,
        crow::response& res,
        context&        ctx
    );

    // after_handle is called after the route handler.
    // No-op for auth middleware.
    void after_handle(
        crow::request&  req,
        crow::response& res,
        context&        ctx
    );

    // Injected at server construction
    std::shared_ptr<PostgresConnector> db;

}

// Adds CORS headers to every response.
// Required for the React frontend on a different origin.
struct CorsMiddleware {

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

// Type alias for the Crow app with both middlewares.
// Defined here once so all handlers share the same type.
using RockPulseApp = crow::App<CorsMiddleware, AuthMiddleware>;

class Server {
public:

    Server(
        const ServerConfig&   server_cfg,
        const DBConfig&       db_cfg,
        const DetectorConfig& detector_cfg
    );

    // Non-copyable
    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    // Blocks until shutdown signal received.
    void run();

    // Exposed for testing
    bool isReady() const;


private:

    // Wires all endpoint handlers to the Crow app.
    // Called once in constructor.

    void registerRoutes();

    // Route handlers - each defined in its own file 
    // under src/api/handlers/
    // Declared here so registerRoutes() can reference them.
    cout << "************************************\n" << endl; 
    // POST /api/v1/detect
    // Multipart image upload -> full inference pipeline
    crow::response handleDetect(
        const crow::request& req,
        AuthMiddleware::context& auth_ctx
    );

    // GET /api/health
    crow::response handleHealth(
        const crow::request& req
    );

    // GET /api/v1/detections
    // Paginated job list for a conveyor
    crow::response handleListDetections(
        const crow::request& req,
        AuthMiddleware::context& auth_ctx
    );

    // GET /api/v1/detections/{job_id}
    // Full detection payload by job_id
    crow::response handleGetDetection(
        const crow::request& req,
        const std::string&   job_id,
        AuthMiddleware::context& auth_ctx
    );

    // HELPERS 

    // Saves uploaded image bytes to inputs_dir/{job_id}.jpg
    // Returns saved file path or empty string on failure.
    std::string saveInputImage(
        const std::string&      job_id,
        const std::string&      image_bytes
    ) const;

    // Saves annotated output image to outputs_dir/{job_id}.png
    std::string saveOutputImage(
        const std::string& job_id,
        const cv::Mat&     annotated_image
    ) const;


    // Serializes DetectionResult + MetrologyReport to JSON
    // for the POST /api/v1/detect response.
    crow::json::wvalue buildDetectResponse(
        const DetectionResult&  result,
        const MetrologyReport&  report,
        const std::string&      job_id,
        const std::string&      conveyor_id,
        const std::string&      location_code
    ) const;


// Serializes a single RockMetrics to crow::json::wvalue
    crow::json::wvalue rockToJson(const RockMetrics& r) const;

    // Generates a RFC 4122 v4 UUID string
    std::string generateUUID() const;

// Members 

    ServerConfig      server_cfg_;
    std::shared_ptr<RockDetector>    detector_;
    std::shared_ptr<PostgresConnector>  db_;
    RockPulseApp         app_;
};

// end of RockPulse namespace 

}