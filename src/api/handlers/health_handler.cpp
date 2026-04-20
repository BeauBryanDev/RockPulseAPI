#include "api/handlers/health_handler.hpp"

namespace RockPulse {
namespace Handlers {

crow::response health(
    const crow::request&    req,
    RockDetector&           detector,
    PostgresConnector&      db)
{
    crow::json::wvalue body;
    body["status"]   = (detector.isReady() && db.isReady()) ? "ok" : "degraded";
    body["model"]    = detector.modelName();
    body["db"]       = db.isReady()       ? "connected" : "error";
    body["detector"] = detector.isReady() ? "ready"     : "error";
    body["version"]  = "1.0.0";

    crow::response res(200, body);
    res.set_header("Content-Type", "application/json");
    return res;
}

} // namespace Handlers
} // namespace RockPulse
