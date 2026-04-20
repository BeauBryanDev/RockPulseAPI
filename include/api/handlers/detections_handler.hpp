#pragma once

#include <crow.h>
#include <string>

#include "api/server.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

crow::response listDetections(
    const crow::request&        req,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db
);

crow::response getDetection(
    const crow::request&        req,
    const std::string&          job_id,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db
);

} // namespace Handlers
} // namespace RockPulse
