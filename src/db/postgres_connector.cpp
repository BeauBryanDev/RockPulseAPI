#include "db/postgres_connector.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
#include <openssl/sha.h>


namespace  RockPulse {

    // Connection Pool 

    ConnectionPool::ConnectionPool(const DBConfig& config)
    : config_(config)
{
    const std::string conn_str = buildConnectionString();

    for (int i = 0; i < config_.pool_size; ++i) {

        try {

            pool_.push(
                std::make_shared<pqxx::connection>(conn_str)
            );

        } catch (const std::exception& e) {

            std::cerr << "[ConnectionPool] Failed to create "
                         "connection " << i << ": "
                      << e.what() << std::endl;
            throw;
        }
    }

    std::cout << "[ConnectionPool] "
              << config_.pool_size
              << " connections established to "
              << config_.dbname << "@"
              << config_.host   << ":"
              << config_.port   << std::endl;
}


std::string ConnectionPool::buildConnectionString() const
{

    return "host="     + config_.host     +
           " port="    + config_.port     +
           " dbname="  + config_.dbname   +
           " user="    + config_.user     +
           " password="+ config_.password +
           " connect_timeout=10";
}


std::shared_ptr<pqxx::connection> ConnectionPool::acquire()
{

    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return !pool_.empty(); });

    auto conn = pool_.front();
    pool_.pop();

    return conn;

}

void ConnectionPool::release(

    std::shared_ptr<pqxx::connection> conn)
{
    {

        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
    }

    cv_.notify_one();
}

int ConnectionPool::poolSize() const
{
    return config_.pool_size;
}

// ScopedConnection

ScopedConnection::ScopedConnection(ConnectionPool& pool)
    : pool_(pool)
    , conn_(pool.acquire())
{}

ScopedConnection::~ScopedConnection()
{
    pool_.release(conn_);
}

pqxx::connection& ScopedConnection::operator*() const
{
    return *conn_;
}

pqxx::connection* ScopedConnection::operator->() const
{
    return conn_.get();
}

// PostgresConnector 

PostgresConnector::PostgresConnector(const DBConfig& config)
    : config_(config)
    , pool_(config)
{}


// SHA-256 via OpenSSL. Returns lowercase hex string.
// Never store raw tokens - only the hash.
std::string PostgresConnector::hashToken(
    const std::string& raw_token)

{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(
        reinterpret_cast<const unsigned char*>(raw_token.c_str()),
        raw_token.size(),
        hash
    );

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        
        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(hash[i]);

    }
    return oss.str();
}


bool PostgresConnector::isReady() const
{
    try {

        ScopedConnection conn(pool_);
        pqxx::work tx(*conn);
        tx.exec1("SELECT 1");
        tx.commit();

        return true;

    } catch (...) {

        return false;
    }
}

// Validate Token 
TokenValidation PostgresConnector::validateToken(

    const std::string& raw_token) const
{
    TokenValidation tv;

    // Strip "Bearer " prefix if present
    std::string token = raw_token;
    const std::string prefix = "Bearer ";

    if (token.rfind(prefix, 0) == 0) {

        token = token.substr(prefix.size());
    }

    const std::string token_hash = hashToken(token);

    try {

        ScopedConnection conn(pool_);
        pqxx::work tx(*conn);

        pqxx::result r = tx.exec_params(
            "SELECT "
            "t.location_id,"
            "l.location_code, l.location_name,"
            "l.avg_rock_density,"
            "l.fi_alpha, l.fi_beta, l.fi_gamma,"
            "c.conveyor_id, c.conveyor_code,"
            "c.base_rock_density,"
            "c.calibration_marker_cm "
        "FROM api_tokens t "
        "JOIN locations l      ON l.location_id = t.location_id "
        "JOIN conveyor_belts c ON c.location_id = t.location_id "
        "WHERE t.token_hash    = $1 "
        "AND t.is_active     = TRUE "
        "AND t.token_exp_date > NOW() "
        "LIMIT 1",
            token_hash
        );

        tx.commit();

        if (r.empty()) {

            tv.valid     = false;
            tv.error_msg = "Invalid or expired token.";

            return tv;
        }

        tv.valid       = true;

        tv.location_id = r[0]["location_id"].as<std::string>();

        tv.location.location_id   = tv.location_id;

        tv.location.location_code =
            r[0]["location_code"].as<std::string>();

        tv.location.location_name =
            r[0]["location_name"].as<std::string>();

        tv.location.avg_rock_density =
            r[0]["avg_rock_density"].as<float>(0.0f);

        tv.location.fi_alpha =
            r[0]["fi_alpha"].as<float>(0.33f);

        tv.location.fi_beta  =
            r[0]["fi_beta"].as<float>(0.33f);

        tv.location.fi_gamma =
            r[0]["fi_gamma"].as<float>(0.34f);

        tv.conveyor_id = r[0]["conveyor_id"].as<std::string>();

        tv.conveyor.conveyor_id   = tv.conveyor_id;
        tv.conveyor.conveyor_code =
            r[0]["conveyor_code"].as<std::string>();
        tv.conveyor.base_rock_density =
            r[0]["base_rock_density"].as<float>(0.0f);
        tv.conveyor.calibration_marker_cm =
            r[0]["calibration_marker_cm"].as<float>(30.0f);

    } catch (const std::exception& e) {

        tv.valid     = false;

        tv.error_msg = std::string("DB error: ") + e.what();

        std::cerr << "[PostgresConnector] validateToken: "
                  << tv.error_msg << std::endl;
    }

    return tv;
}

// insertDetectionJob()
std::string PostgresConnector::insertDetectionJob(

    const std::string& conveyor_id,
    int    rock_count,
    float   calibration_k,
    float    density,
    const std::string& image_path,
    const std::string& output_path) const
{
    ScopedConnection conn(pool_);
    pqxx::work tx(*conn);

    pqxx::result r = tx.exec_params(  // INSERTION QUERY SQL COMMANDS 
        "INSERT INTO detection_jobs "
        "  (conveyor_id, rock_count, calibration_k, "
        "   density, image_path, output_path) "
        "VALUES ($1, $2, $3, $4, $5, $6) "
        "RETURNING job_id::TEXT",
        conveyor_id,
        rock_count,
        calibration_k,
        density,
        image_path,
        output_path
    );

    tx.commit();

    return r[0][0].as<std::string>();
}


// Bulk inserts RockMetrics via stream_to, then queries back the
// generated bigserial IDs so the caller can link rock_clusters.rock_id.
std::vector<int64_t> PostgresConnector::insertRockDetections(

    const std::string&   job_id,
    const std::vector<RockMetrics>& rocks) const
{
    if (rocks.empty()) return {};

    ScopedConnection conn(pool_);
    pqxx::work tx(*conn);

    pqxx::stream_to stream = pqxx::stream_to::table(
        tx,
        {"rock_detections"},
        {
            "job_id",
            "bbox_x", "bbox_y", "bbox_w", "bbox_h",
            "confidence",
            "area_px",
            "area_cm2",
            "L_cm", "W_cm",
            "estimate_H_cm",
            "perimeter_px", "perimeter_cm",
            "equiv_diameter",
            "volume_cm3", "mass_g",
            "aspect_ratio",
            "sphericity",
            "convexity",
            "solidity",
            "convex_area_px",
            "major_axis_cm", "minor_axis_cm",
            "ellipse_angle",
            "fragment_index"
        }
    );

    for (const auto& r : rocks) {
        std::optional<float> fi = (r.fragment_index >= 0.0f)
            ? std::optional<float>(r.fragment_index)
            : std::nullopt;

        stream.write_values(
            job_id,
            r.bbox_x, r.bbox_y, r.bbox_w, r.bbox_h,
            r.confidence,
            r.area_px,
            r.area_cm2,
            r.L_cm, r.W_cm,
            r.estimate_H_cm,
            r.perimeter_px, r.perimeter_cm,
            r.equiv_diameter,
            r.volume_cm3, r.mass_g,
            r.aspect_ratio,
            r.sphericity,
            r.convexity,
            r.solidity,
            r.convex_area_px,
            r.major_axis_cm, r.minor_axis_cm,
            r.ellipse_angle,
            fi
        );
    }

    stream.complete();

    // stream_to has no RETURNING — query back the IDs in insertion order.
    // BIGSERIAL is monotonically increasing so ORDER BY id gives insertion order.
    pqxx::result id_rows = tx.exec_params(
        "SELECT id FROM rock_detections WHERE job_id = $1::UUID ORDER BY id ASC",
        job_id
    );

    tx.commit();

    std::vector<int64_t> ids;
    ids.reserve(id_rows.size());
    for (const auto& row : id_rows) {
        ids.push_back(row[0].as<int64_t>());
    }
    return ids;
}

void PostgresConnector::insertClusterResults(

    const std::string&          job_id,
    const ClusterResult&        result,
    const std::vector<int64_t>& rock_ids) const
{
    if (result.cluster_labels.empty()) return;

    ScopedConnection conn(pool_);
    pqxx::work tx(*conn);

    // Insert rock_clusters — one row per rock, with FK to rock_detections.id
    pqxx::stream_to rock_stream = pqxx::stream_to::table(
        tx,
        {"rock_clusters"},
        {"job_id", "rock_id", "cluster_label"}
    );

    for (std::size_t i = 0; i < result.cluster_labels.size(); ++i) {
        const int64_t rid = (i < rock_ids.size()) ? rock_ids[i] : 0;
        rock_stream.write_values(job_id, rid, result.cluster_labels[i]);
    }
    rock_stream.complete();

    // Insert cluster_centroids — one row per cluster
    for (int k = 0; k < result.num_clusters; ++k) {

        tx.exec_params(
            "INSERT INTO cluster_centroids "
            "  (job_id, cluster_label, "
            "   centroid_L, centroid_W, centroid_vol, "
            "   rock_count) "
            "VALUES ($1, $2, $3, $4, $5, $6)",
            job_id,
            k,
            result.centroid_pca_x[k],
            result.centroid_pca_y[k],
            0.0,
            static_cast<int>(
                std::count(
                    result.cluster_labels.begin(),
                    result.cluster_labels.end(), k
                )
            )
        );
    }

    tx.commit();
}


pqxx::result PostgresConnector::getDetectionJob(

    const std::string& job_id) const
{

    ScopedConnection conn(pool_);
    pqxx::read_transaction tx(*conn);

    return tx.exec_params(

        "SELECT r.* "
        "FROM rock_detections r "
        "WHERE r.job_id = $1 "
        "ORDER BY r.id ASC",
        job_id

    );

}


pqxx::result PostgresConnector::listDetectionJobs(
    const std::string& conveyor_id,
    int                limit,
    int                offset) const
{
    ScopedConnection conn(pool_);
    pqxx::read_transaction tx(*conn);

    return tx.exec_params(
        "SELECT job_id, created_at, rock_count, "
        "       image_path, output_path, density "
        "FROM detection_jobs "
        "WHERE conveyor_id = $1 "
        "ORDER BY created_at DESC "
        "LIMIT $2 OFFSET $3",
        conveyor_id,
        limit,
        offset
    );
}

LocationRow PostgresConnector::getLocationByConveyor(

    const std::string& conveyor_id) const
{
    ScopedConnection conn(pool_);
    pqxx::read_transaction tx(*conn);

    pqxx::result r = tx.exec_params(
        "SELECT l.location_id, l.location_code, "
        "       l.location_name, l.avg_rock_density, "
        "       l.fi_alpha, l.fi_beta, l.fi_gamma "
        "FROM locations l "
        "JOIN conveyor_belts c "
        "  ON c.location_id = l.location_id "
        "WHERE c.conveyor_id = $1",
        conveyor_id
    );

    if (r.empty()) {

        throw std::runtime_error(
            "No location found for conveyor_id: " + conveyor_id
        );
    }

    LocationRow row;

    row.location_id   = r[0]["location_id"].as<std::string>();
    row.location_code = r[0]["location_code"].as<std::string>();
    row.location_name = r[0]["location_name"].as<std::string>();

    row.avg_rock_density =
        r[0]["avg_rock_density"].as<float>(0.0f);

    row.fi_alpha = r[0]["fi_alpha"].as<float>(0.33f);
    row.fi_beta  = r[0]["fi_beta"].as<float>(0.33f);
    row.fi_gamma = r[0]["fi_gamma"].as<float>(0.34f);

    return row;

}


ConveyorRow PostgresConnector::getConveyorById(

    const std::string& conveyor_id) const
{
    ScopedConnection conn(pool_);

    pqxx::read_transaction tx(*conn);

    pqxx::result r = tx.exec_params(

        "SELECT conveyor_id, location_id, conveyor_code, "
        "       base_rock_density, calibration_marker_cm "
        "FROM conveyor_belts "
        "WHERE conveyor_id = $1",
        conveyor_id
    );

    if (r.empty()) {

        throw std::runtime_error(
            
            "Conveyor not found: " + conveyor_id
        );
    }

    ConveyorRow row;

    row.conveyor_id   = r[0]["conveyor_id"].as<std::string>();
    row.location_id   = r[0]["location_id"].as<std::string>();
    row.conveyor_code = r[0]["conveyor_code"].as<std::string>();

    row.base_rock_density =
        r[0]["base_rock_density"].as<float>(0.0f);

    row.calibration_marker_cm =
        r[0]["calibration_marker_cm"].as<float>(30.0f);

    return row;

}

pqxx::result PostgresConnector::getGranulometryData(
    const std::string& job_id,
    const std::string& conveyor_id) const
{
    ScopedConnection conn(pool_);
    pqxx::read_transaction tx(*conn);

    return tx.exec_params(
        "SELECT r.equiv_diameter, r.volume_cm3 "
        "FROM rock_detections r "
        "JOIN detection_jobs j ON j.job_id = r.job_id "
        "WHERE r.job_id    = $1 "
        "  AND j.conveyor_id = $2 "
        "ORDER BY r.equiv_diameter ASC",
        job_id,
        conveyor_id
    );
}

void PostgresConnector::updateConveyorMarker(
    const std::string& conveyor_id,
    float              marker_cm) const
{
    ScopedConnection conn(pool_);
    pqxx::work tx(*conn);

    pqxx::result r = tx.exec_params(
        "UPDATE conveyor_belts "
        "SET calibration_marker_cm = $1 "
        "WHERE conveyor_id = $2 "
        "RETURNING conveyor_id",
        marker_cm,
        conveyor_id
    );

    if (r.empty()) {
        throw std::runtime_error("Conveyor not found: " + conveyor_id);
    }

    tx.commit();
}

// end of RockPulse namespace

}