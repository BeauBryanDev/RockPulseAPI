#pragma once

#include <crow.h>
#include <string>

#include "api/server.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

// GET /api/v1/analytics/granulometry/<job_id>
// Returns D30/D50/D80 and the full cumulative-passing curve
// for one detection job, shaped for a Recharts LineChart.
crow::response granulometry(
    const crow::request&        req,
    const std::string&          job_id,
    AuthMiddleware::context&    auth_ctx,
    PostgresConnector&          db
);

} // namespace Handlers
} // namespace RockPulse
