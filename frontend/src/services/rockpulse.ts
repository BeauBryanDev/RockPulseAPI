import axios from 'axios';

const api = axios.create({
    baseURL: '/api',
    headers: {
        'Content-Type': 'application/json',
    },
});

// Set API key if present in localStorage
api.interceptors.request.use((config) => {
    const token = localStorage.getItem('rockpulse_api_token');
    if (token) {
        config.headers['X-API-KEY'] = token;
    }
    return config;
});

export interface DetectionResult {
    job_id: string;
    conveyor_id: string;
    location_code: string;
    rock_count: number;
    detections: Array<{
        id: number;
        confidence: number;
        area_cm2: number;
        L_cm: number;
        W_cm: number;
        volume_cm3: number;
        mass_g: number;
        bbox: { x: number; y: number; w: number; h: number };
    }>;
    aggregates: {
        total_volume_cm3: number;
        total_mass_g: number;
        mean_equiv_diam: number;
        mean_sphericity: number;
        mean_aspect_ratio: number;
    };
    granulometry: {
        D30: number;
        D50: number;
        D80: number;
        curve: Array<{ d: number; cp: number }>;
    };
    histograms: any;
    clustering: any;
}

export const rockpulseService = {
    getHealth: async () => {
        const response = await api.get('/health');
        return response.data;
    },

    detect: async (imageFile: File): Promise<DetectionResult> => {
        const formData = new FormData();
        formData.append('image', imageFile);

        const response = await api.post('/v1/detect', formData, {
            headers: {
                'Content-Type': 'multipart/form-data',
            },
        });
        return response.data;
    },

    getDetections: async (limit = 20, offset = 0) => {
        const response = await api.get('/v1/detections', {
            params: { limit, offset },
        });
        return response.data;
    },

    getDetectionDetails: async (jobId: string): Promise<DetectionResult> => {
        const response = await api.get(`/v1/detections/${jobId}`);
        return response.data;
    },

    getGranulometry: async (jobId: string) => {
        const response = await api.get(`/v1/analytics/granulometry/${jobId}`);
        return response.data;
    },
};

export default api;
