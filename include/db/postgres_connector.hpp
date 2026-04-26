#pragma once

#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>

#include "inference/metrics_calculator.hpp"
#include "inference/metrology.hpp"


namespace RockPulse {

    struct DBConfig {
    std::string host     = "localhost";
    std::string port     = "5432";
    std::string dbname   = "rockpulse";
    std::string user     = "rockpulse_user";
    std::string password = "";
    int         pool_size = 4;   // min 4 under concurrent Crow load
};

// Twin at the locations table.
// Returned by getLocationByToken() for job context assembly.
struct LocationRow {
    std::string location_id;
    std::string location_code;
    std::string location_name;
    float       avg_rock_density = 0.0f;
    float       fi_alpha         = 0.33f;
    float       fi_beta          = 0.33f;
    float       fi_gamma         = 0.34f;
};

// Mirrors the conveyor_belts table.
// Returned by getConveyorById() for calibration context.
struct ConveyorRow {
    std::string conveyor_id;
    std::string location_id;
    std::string conveyor_code;
    float       base_rock_density     = 0.0f;
    float       calibration_marker_cm = 30.0f;
};

// Result of validateToken().
// On success carries location and conveyor context
// needed to assemble DetectionJob.

struct TokenValidation {
    bool         valid        = false;
    std::string  location_id;
    std::string  conveyor_id;
    LocationRow  location;
    ConveyorRow  conveyor;
    std::string  error_msg;
};

// Thread-safe pool of pqxx::connection objects.
// Crow handles concurrent requests - each request acquires
// a connection, uses it, and returns it to the pool.
// Blocks if all connections are in use (max pool_size).
class ConnectionPool {
public:

    explicit ConnectionPool(const DBConfig& config);
    ~ConnectionPool() = default;

    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    // Acquire a connection from the pool.
    // Blocks until one is available.
    std::shared_ptr<pqxx::connection> acquire();

    // Return a connection to the pool.
    void release(std::shared_ptr<pqxx::connection> conn);

    int poolSize() const;

private:

    DBConfig                                          config_;
    std::queue<std::shared_ptr<pqxx::connection>>    pool_;
    std::mutex                                        mutex_;
    std::condition_variable                           cv_;

    std::string buildConnectionString() const;
};
// RAII wrapper. Acquires on construction, releases on
// destruction. Guarantees connection is always returned
// to the pool even if an exception is thrown.
class ScopedConnection {

public:
    explicit ScopedConnection(ConnectionPool& pool);
    ~ScopedConnection();

    ScopedConnection(const ScopedConnection&)            = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    pqxx::connection& operator*()  const;
    pqxx::connection* operator->() const;

private:

    ConnectionPool&                       pool_;
    std::shared_ptr<pqxx::connection>     conn_;
};

// All SQL operations for RockPulseAPI.
// Owns the ConnectionPool.
// Created ONCE at server startup and shared via shared_ptr.
class PostgresConnector {
public:

    explicit PostgresConnector(const DBConfig& config);

    PostgresConnector(const PostgresConnector&)            = delete;
    PostgresConnector& operator=(const PostgresConnector&) = delete;

    // Pings the DB with a lightweight SELECT 1.
    // Used by GET /api/health.
    bool isReady()  const ;

    // Hashes the raw token, looks up api_tokens table.
    // Returns TokenValidation with location + conveyor context.
    // Called by Crow middleware on every protected request.
    TokenValidation validateToken(const std::string& raw_token) const;

    // Inserts one row into detection_jobs.
    // Returns the generated job_id UUID string.
    // Called before rock insertions so job_id FK is valid.
    std::string insertDetectionJob(
        const std::string& conveyor_id,
        int      rock_count,
        float     calibration_k,
        float   density,
        const std::string& image_path,
        const std::string& output_path
    ) const;

    // Batch inserts all RockMetrics for one job in a single transaction.
    // needed by insertClusterResults to populate rock_clusters.rock_id.
    std::vector<int64_t> insertRockDetections(
        const std::string&              job_id,
        const std::vector<RockMetrics>& rocks
    ) const;  // Returns the DB-generated IDs (bigserial) in insertion order,

    // Inserts rock_clusters and cluster_centroids for one job.
    // rock_ids must match result.cluster_labels index-for-index.
    void insertClusterResults(
        const std::string&          job_id,
        const ClusterResult&        result,
        const std::vector<int64_t>& rock_ids
    ) const;

    // Returns full detection payload for GET /api/detections/{job_id}
    // Joins detection_jobs + rock_detections.
    pqxx::result getDetectionJob(const std::string& job_id) const;

    // Returns paginated list for GET /api/detections.
    // Default page size 20.
    pqxx::result listDetectionJobs(
        const std::string& conveyor_id,
        int     limit  = 20,
        int      offset = 0
    ) const;

    // Returns LocationRow for a given conveyor_id.
    // Used to load fi_params for FI computation.
    LocationRow getLocationByConveyor(
        const std::string& conveyor_id
    ) const;

    // Returns ConveyorRow by conveyor_id.
    // Used to load calibration_marker_cm and density.
    ConveyorRow getConveyorById(
        const std::string& conveyor_id
    ) const;

    // Returns (equiv_diameter, volume_cm3) for all rocks in a job,
    // ordered ascending by equiv_diameter.
    // The conveyor_id JOIN enforces that the caller's token owns the job.
    pqxx::result getGranulometryData(
        const std::string& job_id,
        const std::string& conveyor_id
    ) const;

    // Updates calibration_marker_cm for a conveyor.
    // Throws std::runtime_error if conveyor_id not found.
    void updateConveyorMarker(
        const std::string& conveyor_id,
        float              marker_cm
    ) const;

    // -------------------------------------------------------------------------
    // LOCATIONS (CRUD)
    // -------------------------------------------------------------------------

    // Returns all columns for one location, or empty result if not found.
    pqxx::result getLocationById(const std::string& location_id) const;

    // Inserts a new location. Returns generated location_id UUID string.
    // Throws pqxx::sql_error (sqlstate 23505) on duplicate location_code.
    std::string insertLocation(
        const std::string&   code,
        const std::string&   name,
        const std::string&   description,
        std::optional<float> avg_density,
        float                fi_alpha,
        float                fi_beta,
        float                fi_gamma,
        std::optional<float> latitude,
        std::optional<float> longitude
    ) const;

    // Full update of mutable location fields. Throws if not found.
    void updateLocation(
        const std::string&   location_id,
        const std::string&   name,
        const std::string&   description,
        std::optional<float> avg_density,
        float                fi_alpha,
        float                fi_beta,
        float                fi_gamma,
        std::optional<float> latitude,
        std::optional<float> longitude
    ) const;

    // Updates fi_alpha/beta/gamma only. Throws if not found.
    void patchLocationFIParams(
        const std::string& location_id,
        float alpha, float beta, float gamma
    ) const;

    // Deletes a location (CASCADE: conveyors → jobs → detections). Throws if not found.
    void deleteLocation(const std::string& location_id) const;

    // -------------------------------------------------------------------------
    // CONVEYORS (CRUD additions — patchConveyor via updateConveyorMarker above)
    // -------------------------------------------------------------------------

    // Returns all columns for one conveyor, or empty result if not found.
    pqxx::result getConveyorFull(const std::string& conveyor_id) const;

    // Returns all conveyors for a location, ordered by created_at ASC.
    pqxx::result listConveyorsByLocation(const std::string& location_id) const;

    // Inserts a new conveyor. Returns generated conveyor_id UUID string.
    std::string insertConveyor(
        const std::string&   location_id,
        const std::string&   code,
        std::optional<float> base_rock_density,
        std::optional<float> calibration_marker_cm,
        std::optional<int>   image_rate_minutes,
        const std::string&   note
    ) const;

    // Full update of mutable conveyor fields. Throws if not found.
    void updateConveyor(
        const std::string&   conveyor_id,
        const std::string&   code,
        std::optional<float> base_rock_density,
        std::optional<float> calibration_marker_cm,
        std::optional<int>   image_rate_minutes,
        const std::string&   note
    ) const;

    // Deletes a conveyor (CASCADE: jobs → detections). Throws if not found.
    void deleteConveyor(const std::string& conveyor_id) const;

    // -------------------------------------------------------------------------
    // TOKENS
    // -------------------------------------------------------------------------

    // Hashes raw_token (SHA-256) and inserts into api_tokens for location_id.
    // The raw token is never stored — caller must save it.
    void insertToken(
        const std::string& location_id,
        const std::string& owner_name,
        const std::string& raw_token
    ) const;

    private:
    DBConfig                    config_;
    mutable ConnectionPool      pool_;

    // SHA-256 hash of raw token for lookup in api_tokens
    static std::string hashToken(const std::string& raw_token);
};

// end pof RockPulse namespace 


}