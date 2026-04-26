
// Global TypeScript interfaces mapping to RockPulseAPI JSON contracts.

// Core Geometry & Rock Metrics

export interface BoundingBox {
    x: number;
    y: number;
    w: number;
    h: number;
}

export interface RockDetection {
    id: number;
    confidence: number;
    area_cm2: number;
    L_cm: number;
    W_cm: number;
    estimate_H_cm: number;
    volume_cm3: number;
    mass_g: number;
    perimeter_cm: number;
    equiv_diameter: number;
    aspect_ratio: number;
    sphericity: number;
    convexity: number;
    solidity: number;
    major_axis_cm: number;
    minor_axis_cm: number;
    ellipse_angle: number;
    fragment_index: number | null; // Can be null if FI weights are uncalibrated
    bbox: BoundingBox;
}

// Aggregates & Data Science (BI)

export interface DetectionAggregates {
    total_volume_cm3: number;
    total_mass_g: number;
    mean_equiv_diam: number;
    mean_sphericity: number;
    mean_aspect_ratio: number;
}

export interface GranulometryDataPoint {
    d: number;  // Equivalent diameter in cm (X-axis)
    cp: number; // Cumulative passing fraction 0.0-1.0 (Y-axis)
}

export interface Granulometry {
    D30: number;
    D50: number;
    D80: number;
    curve: GranulometryDataPoint[];
}
export interface HistogramBin {
    from: number;
    to: number;
    count: number;
}

export interface Histogram {
    metric: string;
    mean: number;
    stddev: number;
    min: number;
    max: number;
    bins: HistogramBin[];
}

export interface Histograms {
    equiv_diameter: Histogram;
    volume: Histogram;
    sphericity: Histogram;
    aspect_ratio: Histogram;
}
export interface ScatterPoint {
    rock_id: number;
    x: number;  // PCA1
    y: number;   // PCA2
    cluster: number;
    cluster_name?: string;
}

export interface ClusterCentroid {
    cluster: number;
    x: number;
    y: number;
    name?: string;
}

export interface Clustering {
    num_clusters: number;
    compactness: number;
    pca_variance_pc1: number;
    pca_variance_pc2: number;
    scatter: ScatterPoint[];
    centroids: ClusterCentroid[];
}

// API Endpoint Responses

// POST /api/v1/detect
export interface DetectResponse {
    job_id: string;
    conveyor_id: string;
    location_code: string;
    rock_count: number;
    detections: RockDetection[];
    aggregates: DetectionAggregates;
    granulometry: Granulometry;
    histograms: Histograms;
    clustering: Clustering;
    output_image_url?: string; // Generated image path from the backend
}

// Helper for the detections list
export interface DetectionJobSummary {
    job_id: string;
    created_at: string;
    rock_count: number;
    image_path: string;
    output_path: string;
    density: number;
}

// GET /api/v1/detections
export interface DetectionsListResponse {
    jobs: DetectionJobSummary[];
    limit: number;
    offset: number;
}

// Infrastructure (Locations, Conveyors, Tokens)

export interface Location {
    location_id: string;
    location_code: string;
    location_name: string;
    location_description?: string;
    avg_rock_density: number;
    fi_alpha: number;
    fi_beta: number;
    fi_gamma: number;
    location_latitude?: number;
    location_longitude?: number;
    created_at: string;
}

export interface LocationsResponse {
    locations: Location[];
}

export interface Conveyor {
    conveyor_id: string;
    location_id: string;
    conveyor_code: string;
    base_rock_density: number;
    calibration_marker_cm: number;
    image_rate_minutes: number;
    note_description?: string;
    created_at: string;
}

export interface ConveyorsResponse {
    location_id: string;
    conveyors: Conveyor[];
}

export interface TokenResponse {
    token: string;
    owner_name: string;
    location_id: string;
    warning: string;
}

// Utility & Error Handling

export interface HealthResponse {
    model_ready: boolean;
    db_ready: boolean;
}

export interface ApiError {
    error: string;
    detail?: string;
}