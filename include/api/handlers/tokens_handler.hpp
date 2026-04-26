#pragma once

#include <crow.h>
#include <string>

#include "api/server.hpp"
#include "db/postgres_connector.hpp"

namespace RockPulse {
namespace Handlers {

// POST /api/v1/tokens
// Generates a 72-char raw token, hashes it with SHA-256, stores only the hash.
// Returns the raw token exactly once. The caller must save it immediately.
crow::response createToken(
    const crow::request&   req,
    AuthMiddleware::context& auth_ctx,
    PostgresConnector&       db
);

} // namespace Handlers
} // namespace RockPulse
