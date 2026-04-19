#pragma once

#include <pqxx/pqxx>
#include <string>
#include <vector>
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

    // Bulk inserts all RockMetrics for one job in a single
    // transaction using pqxx::stream_to for maximum throughput.
    // Returns number of rows inserted.
    int insertRockDetections(
        const std::string&              job_id,
        const std::vector<RockMetrics>& rocks
    ) const;

    // Inserts rock_clusters and cluster_centroids for one job.
    // Called after KMeans completes in MetrologyEngine.
    void insertClusterResults(
        const std::string& job_id,
        const ClusterResult& result
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


    private:
    DBConfig                    config_;
    mutable ConnectionPool      pool_;

    // SHA-256 hash of raw token for lookup in api_tokens
    static std::string hashToken(const std::string& raw_token);
};

// end pof RockPulse namespace 


}