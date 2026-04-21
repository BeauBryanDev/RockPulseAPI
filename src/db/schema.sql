-- RockPulseAPI - PostgreSQL Schema
-- Stack: C++17 · Crow · OpenCV · ONNX Runtime · PostgreSQL

-- Enable UUID generation
CREATE EXTENSION IF NOT EXISTS "pgcrypto";


--  LOCATIONS (Mines / Clients)
CREATE TABLE locations (
    location_id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    location_code        VARCHAR(50)  UNIQUE NOT NULL,
    location_name        VARCHAR(100) NOT NULL,
    location_description TEXT,
    avg_rock_density     REAL,        -- average density of the entire mine (kg/m3)
    fi_alpha             REAL DEFAULT 0.33,  -- Fragment Index weight: circularity
    fi_beta              REAL DEFAULT 0.33,  -- Fragment Index weight: solidity
    fi_gamma             REAL DEFAULT 0.34,  -- Fragment Index weight: aspect_ratio
    location_latitude    DECIMAL(10, 8),
    location_longitude   DECIMAL(11, 8),
    created_at           TIMESTAMP DEFAULT NOW()
);

-- CONVEYOR BELTS (Capture Points)

CREATE TABLE conveyor_belts (
    conveyor_id             UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    location_id             UUID NOT NULL REFERENCES locations(location_id) ON DELETE CASCADE,
    conveyor_code           VARCHAR(50) NOT NULL,
    base_rock_density       REAL,        -- density of rocks on this specific belt (kg/m3)
                                         -- this value takes priority over avg_rock_density
    calibration_marker_cm   REAL,        -- ArUco marker physical size in cm (e.g. 30.0)
    image_rate_minutes      INT,         -- capture frequency in minutes
    note_description        TEXT,
    created_at              TIMESTAMP DEFAULT NOW()
);

--  API TOKENS (Security / Auth)

CREATE TABLE api_tokens (
    token_id      UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    location_id   UUID NOT NULL REFERENCES locations(location_id) ON DELETE CASCADE,
    token_hash    VARCHAR(255) UNIQUE NOT NULL,  -- store hash only, never plaintext
    owner_name    VARCHAR(100) NOT NULL,
    is_active     BOOLEAN   DEFAULT TRUE,
    created_at    TIMESTAMP DEFAULT NOW(),
    token_exp_date TIMESTAMP DEFAULT (NOW() + INTERVAL '1 year')
);

--  DETECTION JOBS (Inference Events)

CREATE TABLE detection_jobs (
    job_id        UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conveyor_id   UUID NOT NULL REFERENCES conveyor_belts(conveyor_id) ON DELETE CASCADE,
    rock_count    INT,
    calibration_k REAL NOT NULL,   -- px->cm factor computed from ArUco marker for this job
    density       REAL NOT NULL,  -- copied from conveyor_belts.base_rock_density at job creation
    image_path    TEXT,
    output_path   TEXT,
    created_at    TIMESTAMP DEFAULT NOW()
);

-- ROCK DETECTIONS (Individual Rock Metrics)
-- H estimation: H = 0.4 * (L_cm + W_cm)  [empirical, ellipsoid assumption]
-- Volume:       V = (4 * pi / 3) * L * W * H    [triaxial ellipsoid]
-- Mass:         mass_g = density(kg/m3) * volume_cm3 * 0.001

CREATE TABLE rock_detections (
    id                BIGSERIAL PRIMARY KEY,
    job_id            UUID NOT NULL REFERENCES detection_jobs(job_id) ON DELETE CASCADE,

    -- Bounding box (pixels, original image coordinates)
    bbox_x            INT  NOT NULL,
    bbox_y            INT  NOT NULL,
    bbox_w            INT  NOT NULL,
    bbox_h            INT  NOT NULL,

    -- Detection
    confidence        REAL NOT NULL,
    area_px           INT  NOT NULL,

    -- Dimensional metrics (cm)
    area_cm2          REAL NOT NULL,  -- area_px * k^2
    L_cm              REAL NOT NULL,  -- major axis from minAreaRect
    W_cm              REAL NOT NULL,  -- minor axis from minAreaRect
    estimate_H_cm     REAL NOT NULL,  -- 0.4 * (L_cm + W_cm)
    perimeter_px      REAL NOT NULL,  -- contour arc length in pixels
    perimeter_cm      REAL NOT NULL,  -- perimeter_px * k
    equiv_diameter    REAL NOT NULL,  -- sqrt(4 * area_cm2 / pi)

    -- Volume and mass
    volume_cm3        REAL NOT NULL,  -- (pi/6) * L_cm * W_cm * estimate_H_cm
    mass_g            REAL NOT NULL,  -- density(kg/m3) * volume_cm3 * 0.001

    -- Shape descriptors
    aspect_ratio      REAL NOT NULL,  -- L_cm / W_cm
    sphericity        REAL NOT NULL,  -- Cox Index: (4 * pi * area_px) / perimeter_px^2
    convexity         REAL NOT NULL,  -- perimeter_convex_hull / perimeter_px
    solidity          REAL NOT NULL,  -- area_px / convex_hull_area_px
    convex_area_px    INT  NOT NULL,  -- area of convex hull in pixels

    -- Ellipse axes from fitEllipse (fallback: OBB axes)
    major_axis_cm     REAL,
    minor_axis_cm     REAL,
    ellipse_angle     REAL,

    -- Fragment Index (calibrated per location)
    -- FI = alpha * circularity + beta * solidity + gamma * aspect_ratio
    -- alpha, beta, gamma sourced from locations.fi_alpha/beta/gamma
    fragment_index    REAL,

    created_at        TIMESTAMP DEFAULT NOW()
);

ALTER TABLE rock_detections ADD COLUMN ellipse_angle real;

ALTER TABLE rock_detections RENAME COLUMN l_cm TO "L_cm";
ALTER TABLE rock_detections RENAME COLUMN w_cm TO "W_cm";
ALTER TABLE rock_detections RENAME COLUMN estimate_h_cm TO "estimate_H_cm";
ALTER TABLE rock_detections ADD COLUMN major_axis_cm real;
ALTER TABLE rock_detections ADD COLUMN minor_axis_cm real;


--  ROCK CLUSTERS (KMeans results per job)

CREATE TABLE rock_clusters (
    cluster_id    BIGSERIAL PRIMARY KEY,
    job_id        UUID NOT NULL REFERENCES detection_jobs(job_id) ON DELETE CASCADE,
    rock_id       BIGINT NOT NULL REFERENCES rock_detections(id) ON DELETE CASCADE,
    cluster_label INT  NOT NULL,
    created_at    TIMESTAMP DEFAULT NOW()
);

-- CLUSTER CENTROIDS (Per job, for frontend scatter plot)

CREATE TABLE cluster_centroids (
    centroid_id   BIGSERIAL PRIMARY KEY,
    job_id        UUID NOT NULL REFERENCES detection_jobs(job_id) ON DELETE CASCADE,
    cluster_label INT  NOT NULL,
    centroid_L    REAL NOT NULL,
    centroid_W    REAL NOT NULL,
    centroid_vol  REAL NOT NULL,
    rock_count    INT  NOT NULL,
    created_at    TIMESTAMP DEFAULT NOW()
);


-- INDEXES (for performance)

-- Fast rock lookup by job (primary access pattern for frontend)
CREATE INDEX idx_rocks_job_id
    ON rock_detections(job_id);

-- Composite index for job confidence filtering
CREATE INDEX idx_rocks_job_confidence
    ON rock_detections(job_id, confidence);

-- Historical production reports by conveyor and date
CREATE INDEX idx_jobs_conveyor_date
    ON detection_jobs(conveyor_id, created_at);

-- Token validation in microseconds (Crow middleware)
CREATE INDEX idx_tokens_hash
    ON api_tokens(token_hash);

-- Cluster lookup by job
CREATE INDEX idx_clusters_job_id
    ON rock_clusters(job_id);

CREATE INDEX idx_centroids_job_id
    ON cluster_centroids(job_id);



