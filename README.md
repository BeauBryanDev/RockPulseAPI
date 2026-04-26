# RockPulseAPI

**Industrial Granulometry & Mineralogy via Computer Vision (Deep Learning)**

<div align="center" style=" padding: 2px;">
  <img src="https://img.shields.io/badge/C++-blue?style=flat-square&logo=c%2B%2B&logoColor=white&label=C%2B%2B&labelColor=black&color=blue" alt="C++">
  <img src="https://img.shields.io/badge/Crow-blue?style=flat-square&logo=crow&logoColor=white&label=Crow&labelColor=black&color=blue" alt="Crow">
  <img src="https://img.shields.io/badge/OpenCV-blue?style=flat-square&logo=opencv&logoColor=white&label=OpenCV&labelColor=black&color=blue" alt="OpenCV">
  <img src="https://img.shields.io/badge/ONNX_Runtime-blue?style=flat-square&logo=onnx&logoColor=white&label=ONNX_Runtime&labelColor=black&color=blue" alt="ONNX Runtime">
  <img src="https://img.shields.io/badge/PostgreSQL-blue?style=flat-square&logo=postgresql&logoColor=white&label=PostgreSQL&labelColor=black&color=blue" alt="PostgreSQL">
  <img src="https://img.shields.io/badge/CMake-blue?style=flat-square&logo=cmake&logoColor=white&label=CMake&labelColor=black&color=blue" alt="CMake">
  <img src="https://img.shields.io/badge/Docker-blue?style=flat-square&logo=docker&logoColor=white&label=Docker&labelColor=black&color=blue" alt="Docker">
  <img src="https://img.shields.io/badge/TypeScript-blue?style=flat-square&logo=typescript&logoColor=white&label=TypeScript&labelColor=black&color=blue" alt="TypeScript">
  <img src="https://img.shields.io/badge/React-blue?style=flat-square&logo=react&logoColor=white&label=React&labelColor=black&color=blue" alt="React">
  <img src="https://img.shields.io/badge/Tailwind-blue?style=flat-square&logo=tailwindcss&logoColor=white&label=Tailwind&labelColor=black&color=blue" alt="Tailwind">
  <img src="https://img.shields.io/badge/Recharts-blue?style=flat-square&logo=recharts&logoColor=white&label=Recharts&labelColor=black&color=blue" alt="Recharts">
  <img src="https://img.shields.io/badge/Vite-blue?style=flat-square&logo=vite&logoColor=white&label=Vite&labelColor=black&color=blue" alt="Vite">
</div>

RockPulseAPI is a high-performance B2B web service that uses deep learning to detect and measure rock fragments (coal, stone, ores) on conveyor belts for mining industry. It automates granulometry and mineralogy analysis using instance segmentation, advanced geometric metrology computing by using arUco from opencv2,  it asumes elipsoide geometry for academic purposes, and unsupervised clustering. Built in C++17 with the Crow framework, ONNX Runtime, and PostgreSQL.

---

## Features

- **Deep Learning Inference** — Fine-tuned YOLOv8-nano-seg, mAP50-Box 0.82 / mAP50-Mask 0.83
- **Advanced Metrology** — Real-world volume, sphericity, and Fragment Index (FI) per rock
- **Industrial Analytics** — D30, D50, D80 granulometric curves ready for Recharts
- **Unsupervised Learning** — K-Means clustering and PCA for rock size classification
- **Multi-tenant Security** — SHA-256 hashed API keys, per-location token scoping
- **High Performance** — C++17, thread-safe ONNX session, Crow connection pool

---

## Model

| Property | Value |
|----------|-------|
| Architecture | YOLOv8-nano-seg |
| Training | 50 epochs, mineral fragmentation dataset |
| mAP50 Box | 0.82 |
| mAP50 Mask | 0.83 |
| Input | 640×640 px, NCHW float, normalized [0, 1] |
| Output 0 | `[1, 37, 8400]` — boxes + confidence + 32 mask coefficients |
| Output 1 | `[1, 32, 160, 160]` — prototype mask map |

---

## Metrology Formulas

Pixel-to-real conversions use calibration factor `k` (cm/px) derived from ArUco markers on the conveyor belt.

| Metric | Formula |
|--------|---------|
| Area | `countNonZero(mask) × k²` |
| L, W | `minAreaRect` major/minor axes × k (rotation-invariant OBB) |
| Estimated depth H | `0.4 × (L + W)` |
| Volume | `(π/6) × L × W × H` (triaxial ellipsoid) |
| Mass | `density_kg_m3 × volume_cm3 × 0.001` (grams) |
| Equivalent diameter | `√(4 × area_cm2 / π)` |
| Sphericity (Cox) | `(4π × area_px) / perimeter_px²` |
| Solidity | `area_px / convex_hull_area_px` |
| Convexity | `convex_hull_perimeter / real_perimeter` |
| Aspect ratio | `L / W` |
| Fragment Index | `α × solidity + β × convexity + γ × aspect_ratio`, clamped [0, 1] |

FI returns `null` until `fi_alpha / fi_beta / fi_gamma` are calibrated via `PATCH /api/v1/locations/{id}`.

---

## Security Model

All `/api/v1/*` routes require the `X-API-KEY` header. The `Bearer ` prefix is accepted but optional.

```
X-API-KEY: <raw_token>
```

**How tokens work:**

1. On first deployment, insert a location + conveyor + token directly into the database (bootstrap — see below).
2. After that, use `POST /api/v1/tokens` to issue additional tokens.
3. Every token is scoped to **one location**. A token from location A cannot read or modify location B's data, conveyors, or jobs.
4. Only the SHA-256 hash is stored in `api_tokens`. The raw token is returned exactly once at creation time and never stored.
5. Tokens expire after 1 year (`token_exp_date`). Revoke by setting `is_active = false` directly in the DB.

**Request size limit:** All requests over **20 MB** are rejected with `413` before reaching any handler.

**Error shape — all endpoints return the same structure on failure:**

```json
{ "error": "Human readable message.", "detail": "Technical detail (optional)." }
```

---

## API Reference

### Authentication

| Header | Required | Value |
|--------|----------|-------|
| `X-API-KEY` | Yes (all `/api/v1/*`) | Raw token obtained from `POST /api/v1/tokens` |

---

### Health

#### `GET /api/health` — Public

Returns model and database readiness. No authentication required.

```bash
curl http://localhost:8080/api/health
```

**Response `200`**
```json
{ "model_ready": true, "db_ready": true }
```

---

### Detection

#### `POST /api/v1/detect`

Uploads an image, runs YOLOv8 inference, persists results, returns full metrics.

```bash
curl -X POST http://localhost:8080/api/v1/detect \
  -H "X-API-KEY: <token>" \
  -F "image=@/path/to/image.jpg"
```

**Request:** `multipart/form-data` with field `image` (JPEG / PNG / BMP, max 20 MB).

**Response `200`**
```json
{
  "job_id": "uuid",
  "conveyor_id": "uuid",
  "location_code": "MINE-01",
  "rock_count": 55,
  "detections": [
    {
      "id": 1,
      "confidence": 0.94,
      "area_cm2": 18.4,
      "L_cm": 6.2, "W_cm": 3.8, "estimate_H_cm": 4.0,
      "volume_cm3": 49.3, "mass_g": 131.9,
      "perimeter_cm": 17.1, "equiv_diameter": 4.84,
      "aspect_ratio": 1.63, "sphericity": 0.79,
      "convexity": 0.91, "solidity": 0.88,
      "major_axis_cm": 6.1, "minor_axis_cm": 3.7, "ellipse_angle": 42.0,
      "fragment_index": 0.86,
      "bbox": { "x": 120, "y": 80, "w": 95, "h": 72 }
    }
  ],
  "aggregates": {
    "total_volume_cm3": 2714.2,
    "total_mass_g": 7247.4,
    "mean_equiv_diam": 4.91,
    "mean_sphericity": 0.77,
    "mean_aspect_ratio": 1.58
  },
  "granulometry": {
    "D30": 3.8, "D50": 5.1, "D80": 7.4,
    "curve": [{ "d": 1.2, "cp": 0.05 }, "..."]
  },
  "histograms": { "equiv_diameter": { "..." }, "volume": { "..." } },
  "clustering": { "num_clusters": 3, "scatter": [ "..." ] }
}
```

---

### Detection History

#### `GET /api/v1/detections`

Returns a paginated list of jobs for the token's conveyor.

```bash
curl "http://localhost:8080/api/v1/detections?limit=20&offset=0" \
  -H "X-API-KEY: <token>"
```

**Query params:** `limit` (default 20, max 100) · `offset` (default 0)

**Response `200`**
```json
{
  "jobs": [
    { "job_id": "uuid", "created_at": "2026-04-25T...", "rock_count": 55,
      "image_path": "data/inputs/uuid.jpg", "output_path": "data/outputs/uuid.png",
      "density": 1500.0 }
  ],
  "limit": 20,
  "offset": 0
}
```

#### `GET /api/v1/detections/{job_id}`

Returns full per-rock metrics for one job.

```bash
curl http://localhost:8080/api/v1/detections/<job_id> \
  -H "X-API-KEY: <token>"
```

---

### Analytics

#### `GET /api/v1/analytics/granulometry/{job_id}`

Returns D30 / D50 / D80 and the full cumulative-passing curve, shaped for a Recharts `LineChart`.

```bash
curl http://localhost:8080/api/v1/analytics/granulometry/<job_id> \
  -H "X-API-KEY: <token>"
```

**Response `200`**
```json
{
  "job_id": "uuid",
  "rock_count": 55,
  "D30": 3.8,
  "D50": 5.1,
  "D80": 7.4,
  "curve": [
    { "d": 1.2, "cp": 0.05 },
    { "d": 2.4, "cp": 0.12 },
    "..."
  ]
}
```

`d` = equivalent diameter in cm (x-axis). `cp` = cumulative volume fraction 0.0–1.0 (y-axis).

---

### Locations

Each API token is scoped to one location. Location endpoints enforce that the token's `location_id` matches the resource being accessed.

#### `POST /api/v1/locations`

Creates a new mining site. Requires any valid token (used by admins to onboard new sites).

```bash
curl -X POST http://localhost:8080/api/v1/locations \
  -H "X-API-KEY: <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "location_code": "MINE-02",
    "location_name": "North Pit",
    "location_description": "Copper ore extraction zone",
    "avg_rock_density": 2700.0,
    "location_latitude": -22.9068,
    "location_longitude": -43.1729
  }'
```

**Required:** `location_code`, `location_name`
**Optional:** `location_description`, `avg_rock_density`, `fi_alpha`, `fi_beta`, `fi_gamma`, `location_latitude`, `location_longitude`

**Response `201`**
```json
{ "location_id": "uuid", "location_code": "MINE-02", "location_name": "North Pit" }
```

**Error `409`** — `location_code` already exists.

---

#### `GET /api/v1/locations`

Returns the location associated with the calling token.

```bash
curl http://localhost:8080/api/v1/locations \
  -H "X-API-KEY: <token>"
```

**Response `200`**
```json
{
  "locations": [
    {
      "location_id": "uuid", "location_code": "MINE-01",
      "location_name": "South Belt", "location_description": "Coal extraction",
      "avg_rock_density": 1400.0,
      "fi_alpha": 0.33, "fi_beta": 0.33, "fi_gamma": 0.34,
      "location_latitude": -23.5505, "location_longitude": -46.6333,
      "created_at": "2026-01-15T10:30:00"
    }
  ]
}
```

---

#### `GET /api/v1/locations/{location_id}`

Returns one location. Returns `403` if the token does not own this location.

```bash
curl http://localhost:8080/api/v1/locations/<location_id> \
  -H "X-API-KEY: <token>"
```

---

#### `PUT /api/v1/locations/{location_id}`

Full update of all mutable fields. `location_code` is immutable after creation.

```bash
curl -X PUT http://localhost:8080/api/v1/locations/<location_id> \
  -H "X-API-KEY: <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "location_name": "North Pit (Updated)",
    "avg_rock_density": 2800.0,
    "fi_alpha": 0.40, "fi_beta": 0.35, "fi_gamma": 0.25
  }'
```

**Required:** `location_name` **Returns:** full location object.

---

#### `PATCH /api/v1/locations/{location_id}`

Updates only the Fragment Index calibration weights. Use this after running regression on real rocks. The three values must sum to 1.0 (tolerance ±0.01).

```bash
curl -X PATCH http://localhost:8080/api/v1/locations/<location_id> \
  -H "X-API-KEY: <token>" \
  -H "Content-Type: application/json" \
  -d '{ "fi_alpha": 0.40, "fi_beta": 0.35, "fi_gamma": 0.25 }'
```

**Required:** `fi_alpha`, `fi_beta`, `fi_gamma` — all must be ≥ 0 and sum to 1.0.

**Response `200`**
```json
{ "location_id": "uuid", "fi_alpha": 0.40, "fi_beta": 0.35, "fi_gamma": 0.25 }
```

**Error `422`** — sum deviates from 1.0 by more than 0.01.

---

#### `DELETE /api/v1/locations/{location_id}`

Deletes the location and all dependent data (conveyors, jobs, detections — cascade).

```bash
curl -X DELETE http://localhost:8080/api/v1/locations/<location_id> \
  -H "X-API-KEY: <token>"
```

**Response `200`**
```json
{ "message": "Location deleted.", "location_id": "uuid" }
```

---

### Conveyors

Conveyors belong to a location. All conveyor operations verify the conveyor's `location_id` matches the calling token's location.

#### `POST /api/v1/conveyors`

Creates a new conveyor under the token's location.

```bash
curl -X POST http://localhost:8080/api/v1/conveyors \
  -H "X-API-KEY: <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "conveyor_code": "BELT-03",
    "base_rock_density": 1500.0,
    "calibration_marker_cm": 30.0,
    "image_rate_minutes": 5,
    "note_description": "Primary coal belt, west section"
  }'
```

**Required:** `conveyor_code`
**Optional:** `base_rock_density`, `calibration_marker_cm`, `image_rate_minutes`, `note_description`

**Response `201`**
```json
{ "conveyor_id": "uuid", "location_id": "uuid", "conveyor_code": "BELT-03" }
```

---

#### `GET /api/v1/conveyors`

Returns all conveyors for the token's location, ordered by creation date.

```bash
curl http://localhost:8080/api/v1/conveyors \
  -H "X-API-KEY: <token>"
```

**Response `200`**
```json
{
  "location_id": "uuid",
  "conveyors": [
    {
      "conveyor_id": "uuid", "location_id": "uuid",
      "conveyor_code": "BELT-01",
      "base_rock_density": 1500.0,
      "calibration_marker_cm": 30.0,
      "image_rate_minutes": 5,
      "note_description": "Primary coal belt",
      "created_at": "2026-01-15T10:30:00"
    }
  ]
}
```

---

#### `GET /api/v1/conveyors/{conveyor_id}`

Returns one conveyor. Returns `403` if the conveyor does not belong to the token's location.

```bash
curl http://localhost:8080/api/v1/conveyors/<conveyor_id> \
  -H "X-API-KEY: <token>"
```

---

#### `PUT /api/v1/conveyors/{conveyor_id}`

Full update of all mutable conveyor fields.

```bash
curl -X PUT http://localhost:8080/api/v1/conveyors/<conveyor_id> \
  -H "X-API-KEY: <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "conveyor_code": "BELT-01-UPDATED",
    "base_rock_density": 1600.0,
    "calibration_marker_cm": 25.0,
    "image_rate_minutes": 10,
    "note_description": "Updated belt configuration"
  }'
```

**Required:** `conveyor_code` **Returns:** full conveyor object.

---

#### `PATCH /api/v1/conveyors/{conveyor_id}`

Updates `calibration_marker_cm` only — the physical size of the ArUco marker on this belt.

```bash
curl -X PATCH http://localhost:8080/api/v1/conveyors/<conveyor_id> \
  -H "X-API-KEY: <token>" \
  -H "Content-Type: application/json" \
  -d '{ "calibration_marker_cm": 25.0 }'
```

**Response `200`**
```json
{ "conveyor_id": "uuid", "calibration_marker_cm": 25.0 }
```

---

#### `DELETE /api/v1/conveyors/{conveyor_id}`

Deletes a conveyor and all its detection jobs (cascade).

```bash
curl -X DELETE http://localhost:8080/api/v1/conveyors/<conveyor_id> \
  -H "X-API-KEY: <token>"
```

**Response `200`**
```json
{ "message": "Conveyor deleted.", "conveyor_id": "uuid" }
```

---

### Tokens

#### `POST /api/v1/tokens`

Generates a new API token for the calling token's location. The raw token is returned **once** — it is never stored. The caller must save it immediately.

```bash
curl -X POST http://localhost:8080/api/v1/tokens \
  -H "X-API-KEY: <existing_token>" \
  -H "Content-Type: application/json" \
  -d '{ "owner_name": "conveyor-belt-03-client" }'
```

**Required:** `owner_name`

**Response `201`**
```json
{
  "token": "a1b2c3d4-...-e5f6a7b8-...",
  "owner_name": "conveyor-belt-03-client",
  "location_id": "uuid",
  "warning": "Save this token now. It will never be shown again."
}
```

The generated token is 72 characters (two UUID4s concatenated). Only the SHA-256 hash is persisted in `api_tokens`. Tokens expire after 1 year.

---

## Endpoint Summary

| Method | Endpoint | Auth | Description |
|--------|----------|------|-------------|
| GET | `/api/health` | Public | Model + DB readiness |
| POST | `/api/v1/detect` | Required | Image inference → DB insert → full metrics JSON |
| GET | `/api/v1/detections` | Required | Paginated job list for token's conveyor |
| GET | `/api/v1/detections/{job_id}` | Required | Full per-rock metrics for one job |
| GET | `/api/v1/analytics/granulometry/{job_id}` | Required | D30/D50/D80 + cumulative-passing curve |
| POST | `/api/v1/locations` | Required | Create mining site |
| GET | `/api/v1/locations` | Required | List token's own location |
| GET | `/api/v1/locations/{id}` | Required | Get one location |
| PUT | `/api/v1/locations/{id}` | Required | Full update of location |
| PATCH | `/api/v1/locations/{id}` | Required | Update FI calibration weights only |
| DELETE | `/api/v1/locations/{id}` | Required | Delete location + cascade |
| POST | `/api/v1/conveyors` | Required | Create conveyor under token's location |
| GET | `/api/v1/conveyors` | Required | List all conveyors for token's location |
| GET | `/api/v1/conveyors/{id}` | Required | Get one conveyor |
| PUT | `/api/v1/conveyors/{id}` | Required | Full update of conveyor |
| PATCH | `/api/v1/conveyors/{id}` | Required | Update calibration marker size only |
| DELETE | `/api/v1/conveyors/{id}` | Required | Delete conveyor + cascade |
| POST | `/api/v1/tokens` | Required | Generate new token (shown once, never stored) |

---

## Database Schema (7 tables)

| Table | Purpose |
|-------|---------|
| `api_tokens` | SHA-256 hashed API keys, scoped per location, 1-year TTL |
| `locations` | Mining sites with FI calibration weights and coordinates |
| `conveyor_belts` | Capture points — density, ArUco marker size, capture rate |
| `detection_jobs` | One row per inference run — calibration k, image paths |
| `rock_detections` | Per-rock metrics for every detection job |
| `rock_clusters` | K-Means cluster assignment per rock |
| `cluster_centroids` | Cluster centroids for scatter plot visualization |

---

## Build

### Local Development

```bash
# Prerequisites: libpqxx, OpenCV, OpenSSL, nlohmann-json, pkg-config
sudo apt install libpqxx-dev libopencv-dev libssl-dev nlohmann-json3-dev

# Configure and build
mkdir -p build && cd build
cmake ..
cmake --build . --parallel 8

# Run (LD_LIBRARY_PATH needed for bundled ONNX Runtime)
cd ..
LD_LIBRARY_PATH=./ort/lib:$LD_LIBRARY_PATH ./build/RockPulseAPI
```

### Docker

```bash
docker-compose up --build
```

The compose stack starts PostgreSQL, applies `src/db/schema.sql` on first boot, then starts the API on port 8080.

---

## Bootstrap (First-Time Setup)

Because all API endpoints require authentication, the first location, conveyor, and token must be seeded directly into the database:

```sql
-- 1. Insert location
INSERT INTO locations (location_code, location_name, avg_rock_density)
VALUES ('MINE-01', 'South Belt', 1500.0);

-- 2. Insert conveyor (use the location_id from step 1)
INSERT INTO conveyor_belts (location_id, conveyor_code, calibration_marker_cm, base_rock_density)
VALUES ('<location_id>', 'BELT-01', 30.0, 1500.0);

-- 3. Insert first token (SHA-256 of your chosen raw token)
-- Generate hash: echo -n "your_raw_token" | sha256sum
INSERT INTO api_tokens (location_id, token_hash, owner_name)
VALUES ('<location_id>', '<sha256_hash>', 'admin');
```

After the first token exists, use `POST /api/v1/tokens` for all subsequent token creation.

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| HTTP Framework | Crow (header-only) |
| Inference | ONNX Runtime C++ SDK v1.21.0 |
| Image Processing | OpenCV 4.x (pre/post-processing only) |
| Database | PostgreSQL + libpqxx |
| Auth | SHA-256 hashed API keys (OpenSSL) |
| Build | CMake 3.10+ |
| Containers | Docker + docker-compose |
| Frontend (planned) | React + TypeScript + Vite + Tailwind + Recharts |
| Cloud (planned) | AWS |

---

## Project Structure

```
RockPulsePoC/
├── CMakeLists.txt
├── Dockerfile / docker-compose.yml
├── config/
│   ├── app_settings.json          # local dev
│   └── docker_settings.json       # Docker (host: db)
├── models/
│   └── yolo_v8_nano_seg.onnx
├── ort/                            # ONNX Runtime SDK (local only)
├── third_party/                    # Crow header-only library
├── include/
│   ├── api/
│   │   ├── server.hpp             # ServerConfig, AuthMiddleware, CorsMiddleware
│   │   └── handlers/
│   │       ├── detect_handler.hpp
│   │       ├── detections_handler.hpp
│   │       ├── granulometry_handler.hpp
│   │       ├── health_handler.hpp
│   │       ├── location_handler.hpp
│   │       ├── conveyors_handler.hpp
│   │       └── tokens_handler.hpp
│   ├── db/
│   │   └── postgres_connector.hpp
│   ├── inference/
│   │   ├── rock_detector.hpp
│   │   ├── metrics_calculator.hpp
│   │   └── metrology.hpp
│   └── utils/
│       └── image_utils.hpp
└── src/
    ├── main.cpp
    ├── api/
    │   ├── server.cpp
    │   └── handlers/              # one .cpp per handler
    ├── db/
    │   ├── postgres_connector.cpp
    │   └── schema.sql
    ├── inference/
    │   ├── rock_detector.cpp
    │   ├── metrics_calculator.cpp
    │   └── metrology.cpp
    └── utils/
        └── image_utils.cpp
```

---

## License

[MIT License](LICENSE)

## Author

Developed by Bryan Beau — 2026
AI Engineer & Software Architecture Student
