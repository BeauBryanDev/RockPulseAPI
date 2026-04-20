#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <csignal>
#include <filesystem>

#include <crow.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "inference/rock_detector.hpp"
#include "db/postgres_connector.hpp"


// Entry point: load config, boot shared resources, start server

namespace fs = std::filesystem;
using json   = nlohmann::json;


static void signalHandler(int signum)
{
    std::cout << "\n[RockPulseAPI] Signal "
              << signum
              << " received. Shutting down..." << std::endl;
    std::exit(EXIT_SUCCESS);
}

// Reads config/app_settings.json.
// Fills ServerConfig, DBConfig, DetectorConfig.

static void loadConfig(

    const std::string&          config_path,
    RockPulse::ServerConfig&    srv_cfg,
    RockPulse::DBConfig&        db_cfg,
    RockPulse::DetectorConfig&  det_cfg

)  {
    if (!fs::exists(config_path)) {

        throw std::runtime_error(

            "Config file not found: " + config_path
        );
    }

    std::ifstream ifs(config_path);

    if (!ifs.is_open()) {

        throw std::runtime_error(

            "Cannot open config file: " + config_path
        );
    }

    json cfg;
    try {

        ifs >> cfg;

    } catch (const json::parse_error& e) {
        // Throws std::runtime_error if file is missing or malformed.
        throw std::runtime_error(

            std::string("Config JSON parse error: ") + e.what()
        );
    }

    // Server config
    if (cfg.contains("server")) {

        const auto& s = cfg["server"];
        srv_cfg.port         = s.value("port",        8080);
        srv_cfg.concurrency  = s.value("concurrency",    4);
        srv_cfg.enable_cors  = s.value("enable_cors",  true);
        srv_cfg.outputs_dir  = s.value("outputs_dir",
                                    "data/outputs");
        srv_cfg.inputs_dir   = s.value("inputs_dir",
                                    "data/inputs");
        srv_cfg.log_level    = s.value("log_level",   "info");
    }

    // Database config
    if (cfg.contains("database")) {

        const auto& d = cfg["database"];
        db_cfg.host      = d.value("host",     "localhost");
        db_cfg.port      = d.value("port",          "5432");
        db_cfg.dbname    = d.value("dbname",   "rockpulse");
        db_cfg.user      = d.value("user", "rockpulse_user");
        db_cfg.password  = d.value("password",          "");
        db_cfg.pool_size = d.value("pool_size",           4);
    }


    // Detector config
    if (cfg.contains("detector")) {

        const auto& d = cfg["detector"];
        det_cfg.model_path      = d.value("model_path",
            "models/yolo_v8_nano_seg.onnx");
        det_cfg.input_w         = d.value("input_w",         640);
        det_cfg.input_h         = d.value("input_h",         640);
        det_cfg.conf_threshold  = d.value("conf_threshold", 0.80f);
        det_cfg.nms_threshold   = d.value("nms_threshold",  0.20f);
        det_cfg.intra_threads   = d.value("intra_threads",     1);
    }
}


static void printBanner(
    const RockPulse::ServerConfig&   srv_cfg,
    const RockPulse::DBConfig&       db_cfg,
    const RockPulse::DetectorConfig& det_cfg)
{
    std::cout << R"(
 ____            _    ____        _          _    ____ ___
|  _ \ ___   ___| | _|  _ \ _  _| |___  ___/ \  |  _ \_ _|
| |_) / _ \ / __| |/ / |_) | | | | / __|/ _ \_  | |_) | |
|  _ < (_) | (__|   <|  __/| |_| | \__ \  __/ | |  __/| |
|_| \_\___/ \___|_|\_\_|    \__,_|_|___/\___|_| |_|  |___|

  Rock Detection and Granulometry API
  Mining Computer Vision - Academic Portfolio Project
)" << std::endl;

    std::cout << "  Model      : " << det_cfg.model_path    << "\n"
              << "  DB         : " << db_cfg.dbname
              << "@"               << db_cfg.host
              << ":"               << db_cfg.port            << "\n"
              << "  Port       : " << srv_cfg.port           << "\n"
              << "  Threads    : " << srv_cfg.concurrency    << "\n"
              << "  Outputs    : " << srv_cfg.outputs_dir    << "\n"
              << std::endl;
}


// Checks that critical files and dirs exist before booting.
// Fails fast with a clear message rather than a cryptic error
// deep inside the server.
static bool validatePaths(

    const RockPulse::ServerConfig&   srv_cfg,
    const RockPulse::DetectorConfig& det_cfg

) {
    bool ok = true;

    if (!fs::exists(det_cfg.model_path)) {

        std::cerr << "[startup] ONNX model not found: "
                  << det_cfg.model_path << std::endl;
        ok = false;
    }

    for (const auto& dir : {

        srv_cfg.outputs_dir,
        srv_cfg.inputs_dir
    }) {
        std::error_code ec;
        fs::create_directories(dir, ec);

        if (ec) {

            std::cerr << "[startup] Cannot create directory: "
                      << dir << " - " << ec.message() << std::endl;
            ok = false;
        }
    }

    return ok;
}

int main(int argc, char* argv[])
{

    //  Register signal handlers

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    //  Resolve config path
    // Default: config/app_settings.json

    const std::string config_path = (argc > 1)
        ? std::string(argv[1])
        : "config/app_settings.json";

    // Override: first CLI argument
    //   ./RockPulseAPI config/docker_settings.json

    std::cout << "[RockPulseAPI] Loading config: "
              << config_path << std::endl;

    // Load configuration
    RockPulse::ServerConfig   srv_cfg;
    RockPulse::DBConfig       db_cfg;
    RockPulse::DetectorConfig det_cfg;

    try {

        loadConfig(config_path, srv_cfg, db_cfg, det_cfg);

    } catch (const std::exception& e) {

        std::cerr << "[RockPulseAPI] Config error: "
                  << e.what() << std::endl;

        return EXIT_FAILURE;
    }


    // Print banner

    printBanner(srv_cfg, db_cfg, det_cfg);

    //Validate critical paths before allocating resources

    if (!validatePaths(srv_cfg, det_cfg)) {

        std::cerr << "[RockPulseAPI] Startup validation failed. "
                     "Aborting." << std::endl;

        return EXIT_FAILURE;
    }


    //  Boot server
    // Server constructor:
    //   - Creates PostgresConnector (connection pool)
    //   - Creates RockDetector (loads ONNX session once)
    //   - Injects db into AuthMiddleware
    //   - Registers all Crow routes
    try {

        RockPulse::Server server(srv_cfg, db_cfg, det_cfg);

        if (!server.isReady()) {

            std::cerr << "[RockPulseAPI] Server failed readiness "
                         "check. Check DB connection and model "
                         "path." << std::endl;

            return EXIT_FAILURE;
        }

        std::cout << "[RockPulseAPI] All systems ready. "
                     "Accepting connections." << std::endl;

        // Blocks until SIGINT / SIGTERM
        server.run();

    } catch (const std::exception& e) {

        std::cerr << "[RockPulseAPI] Fatal error: "
                  << e.what() << std::endl;

        return EXIT_FAILURE;
    }

    std::cout << "[RockPulseAPI] Goodbye." << std::endl;
    
    return EXIT_SUCCESS;
}

