#include "api/handlers/health_handler.hpp"

namespace RockPulse {
namespace Handlers {

crow::response health(
    const crow::request&    req,
    RockDetector&           detector,
    PostgresConnector&      db)
{
    crow::json::wvalue body;
    body["status"]      = detector.isReady() && db.isReady();
    body["model_ready"] = detector.isReady();
    body["db_ready"]    = db.isReady();
    body["version"]     = "1.0.0";

    crow::response res(200, body);
    res.set_header("Content-Type", "application/json");
    return res;
}

} // namespace Handlers
} // namespace RockPulse
