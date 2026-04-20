#pragma once

#include <crow.h>
#include <string>

#include "api/server.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

crow::response patchConveyor(
    const crow::request&        req,
    const std::string&          conveyor_id,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db
);

} // namespace Handlers
} // namespace RockPulse
