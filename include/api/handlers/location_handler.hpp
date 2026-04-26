#pragma once

#include <crow.h>
#include <string>

#include "api/server.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

// POST /api/v1/locations
crow::response createLocation(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

// GET /api/v1/locations  — returns token's own location
crow::response listLocations(
    const crow::request&     req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

// GET /api/v1/locations/<id>
crow::response getLocation(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

// PUT /api/v1/locations/<id>  — full update of mutable fields
crow::response updateLocation(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

// PATCH /api/v1/locations/<id>  — update fi_alpha, fi_beta, fi_gamma only
crow::response patchLocationFI(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

// DELETE /api/v1/locations/<id>
crow::response deleteLocation(
    const crow::request&     req,
    const std::string&       location_id,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

} // namespace Handlers
} // namespace RockPulse
