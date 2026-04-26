#pragma once

#include <crow.h>
#include <string>

#include "api/server.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

// POST /api/v1/conveyors
crow::response createConveyor(
    const crow::request& req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&  db
);

// GET /api/v1/conveyors  — all conveyors for token's location
crow::response listConveyors(
    const crow::request&  req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&   db
);

// GET /api/v1/conveyors/<id>
crow::response getConveyor(
    const crow::request&   req,
    const std::string&   conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&  db
);

// PUT /api/v1/conveyors/<id>  — full update of mutable fields
crow::response updateConveyor(
    const crow::request&   req,
    const std::string&   conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&    db
);

// PATCH /api/v1/conveyors/<id>  — update calibration_marker_cm only
crow::response patchConveyor(
    const crow::request&   req,
    const std::string&   conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&    db
);

// DELETE /api/v1/conveyors/<id>
crow::response deleteConveyor(
    const crow::request&  req,
    const std::string&     conveyor_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&    db
);

} // namespace Handlers
} // namespace RockPulse
