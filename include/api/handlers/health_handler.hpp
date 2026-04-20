#pragma once

#include <crow.h>

#include "api/server.hpp"
#include "inference/rock_detector.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

crow::response health(
    const crow::request&    req,
    RockDetector&           detector,
    PostgresConnector&      db
);

} // namespace Handlers
} // namespace RockPulse
