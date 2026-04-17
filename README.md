# **RockPulseAPI** 🪨💎

**Industrial Granulometry & Mineralogy via Computer Vision (Deep Learning)**

<div align="center" style=" padding: 2px;">
  <img src="https://img.shields.io/badge/C++-blue?style=flat-square&logo=c%2B%2B&logoColor=white&label=C%2B%2B&labelColor=black&color=blue" alt="C++">
  <img src="https://img.shields.io/badge/Crow-blue?style=flat-square&logo=crow&logoColor=white&label=Crow&labelColor=black&color=blue" alt="Crow">
  <img src="https://img.shields.io/badge/OpenCV-blue?style=flat-square&logo=opencv&logoColor=white&label=OpenCV&labelColor=black&color=blue" alt="OpenCV">
  <img src="https://img.shields.io/badge/ONNX_Runtime-blue?style=flat-square&logo=onnx&logoColor=white&label=ONNX_Runtime&labelColor=black&color=blue" alt="ONNX Runtime">  
  <img src="https://img.shields.io/badge/PostgreSQL-blue?style=flat-square&logo=postgresql&logoColor=white&label=PostgreSQL&labelColor=black&color=blue" alt="PostgreSQL">
  <img src="https://img.shields.io/badge/CMake-blue?style=flat-square&logo=cmake&logoColor=white&label=CMake&labelColor=black&color=blue" alt="CMake">
  <img src="https://img.shields.io/badge/Git-blue?style=flat-square&logo=git&logoColor=white&label=Git&labelColor=black&color=blue" alt="Git">
  <img src="https://img.shields.io/badge/Docker-blue?style=flat-square&logo=docker&logoColor=white&label=Docker&labelColor=black&color=blue" alt="Docker">
  <img src="https://img.shields.io/badge/Axios-blue?style=flat-square&logo=axios&logoColor=white&label=Axios&labelColor=black&color=blue" alt="Axios">
  <img src="https://img.shields.io/badge/TypeScript-blue?style=flat-square&logo=typescript&logoColor=white&label=TypeScript&labelColor=black&color=blue" alt="TypeScript">
  <img src="https://img.shields.io/badge/React-blue?style=flat-square&logo=react&logoColor=white&label=React&labelColor=black&color=blue" alt="React">
  <img src="https://img.shields.io/badge/Tailwind-blue?style=flat-square&logo=tailwindcss&logoColor=white&label=Tailwind&labelColor=black&color=blue" alt="Tailwind">
  <img src="https://img.shields.io/badge/Recharts-blue?style=flat-square&logo=recharts&logoColor=white&label=Recharts&labelColor=black&color=blue" alt="Recharts">
  <img src="https://img.shields.io/badge/Vite-blue?style=flat-square&logo=vite&logoColor=white&label=Vite&labelColor=black&color=blue" alt="Vite">
</div>

<div align="center">
  <img src="./frontend/src/assets/stone_rock.svg" alt="crack_detection" width="60%">
</div>


RockPulseAPI is a high-performance B"B web service that uses machine learning to detect and classify rocks in images for the mining inudstry. It automates the analysis of mineral fragments (coal, stone, ores) on conveyor belts using instance segmentation and advanced geometric metrology.. It is built using the [C++17](https://en.wikipedia.org/wiki/C%2B%2B17) programming language and the [Crow](https://crow.io/) web framework. The inference is performed using [OpenCV](https://opencv.org/) and [ONNX Runtime](https://onnxruntime.ai/).

## Features

- Detect rocks in images
- Classify rocks into different types
- Store rock detections and classifications in a database
- Serve the results via a REST API
- Deploy the service to a cloud platform like [AWS](https://aws.amazon.com/)
- Deep Learning Inference: Powered by a fine-tuned YOLOv8-nano-seg model.
- Advanced Metrology: - Real-time calculation of volume, sphericity, and fragment index ($FI$).- - Industrial Analytics: Generation of $D_{30}, D_{50}, D_{80}$ granulometric curves.Unsupervised Learning: K-Means clustering and PCA for rock classification.
- Enterprise Security: API Key-based authentication (SHA-256) for multi-tenant SaaS architecture.- High Performance: Built with C++17, Crow Framework, and ONNX Runtime.

## Model Performance
The core of the system is a YOLOv8-nano-seg model trained specifically for mineral fragmentation.

- Training: 50 Epochs.

- mAP50 (Box): 0.82

- mAP50 (Mask): 0.83

- Input Size: 640x640 px.

##  Advanced Metrology & Mathematics

The system converts pixel-level data into real-world physical metrics using a dynamic calibration factor $k$ (cm/px) via ArUco markers or reference rulers.

### Geometric FormulasOriented Bounding Box (OBB): 

- Calculated using cv::minAreaRect to obtain true Length ($L$) and Width ($W$) regardless of rotation.
- Estimated Depth ($H$): $H = 0.4 \cdot (L + W)$
- Ellipsoidal Volume ($V$): $V = \frac{\pi}{6} \cdot L \cdot W \cdot H$Sphericity (Cox Index): $S = \frac{4\pi A}{P^2}$Convexity: $\frac{\text{Convex Hull Perimeter}}{\text{Real Perimeter}}$Fragment Index ($FI$): $\alpha \cdot Solidity + \beta \cdot 
- Convexity + \gamma \cdot AspectRatio$
- Fragment Index ($FI$): $\alpha \cdot Solidity + \beta \cdot Convexity + \gamma \cdot AspectRatio$

## SQL Database Schema (PostgreSQL)
The system uses a relational model with 7 tables to ensure scalability and data integrity.

- api_tokens: Manages SHA-256 hashed API Keys for secure client access.
- locations: Stores mining site metadata (DarkCoal, etc.) and geographic coordinates.
- conveyor_belts: Capture points for mineral samples.
- detection_jobs: Inference jobs for each capture point.
- rock_detections: Individual rock detections.
- rock_clusters: Clusters of rocks detected in a single inference job.
- cluster_centroids: Centroids of rock clusters for scatter plot visualization.

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | System status and model metadata. |
| POST | `/v1/detect` | Upload image → Process → Save to DB → Return JSON. |
| GET | `/v1/jobs/{id}` | Retrieve full historical detection data. |
| GET | `/v1/analytics/granulometry/{id}` | Returns D₃₀, D₅₀, D₈₀ data for Recharts. |
| PATCH | `/v1/locations/{id}` | Update site metadata (Coordinates are immutable). |

## Tech Stack

RockPulseAPI is built using the following technologies:

- [C++17](https://en.wikipedia.org/wiki/C%2B%2B17) programming language and the [Crow](https://crow.io/) web framework.
- [OpenCV](https://opencv.org/) and [ONNX Runtime](https://onnxruntime.ai/) for image processing and inference.
- [PostgreSQL](https://www.postgresql.org/) for a relational database.
- [CMake](https://cmake.org/) for project configuration and build automation.
- [Git](https://git-scm.com/) for version control.
- [Docker](https://www.docker.com/) for containerization and deployment.
- [Axios](https://github.com/axios/axios) for HTTP requests.
- [TypeScript](https://www.typescriptlang.org/) for client-side TypeScript.
- [React](https://reactjs.org/) for the frontend UI.
- [Tailwind](https://tailwindcss.com/) for a modern CSS framework.
- [Recharts](https://recharts.org/en-US/) for data visualization.
- [Vite](https://vitejs.dev/) for a fast frontend development environment.  
- [Docker](https://www.docker.com/) for containerization and deployment.
- [AWS](https://aws.amazon.com/) for cloud hosting.

## Project Structure

The project is organized into the following directories:

```
├── build/
├── CMakeLists.txt
├── config/
├── data/
├── debugging.md
├── docker-compose.yml
├── Dockerfile
├── frontend/
├── images
│   ├── rock_03_nms.png
│   └── rock_03.png
├── include
│   ├── api
│   │   ├── handlers/
│   │   └── server.hpp
│   ├── db
│   │   └── postgres_connector.hpp
│   ├── inference
│   │   ├── metrics_calculator.hpp
│   │   └── rock_detector.hpp
│   └── utils
│       └── image_utils.hpp
├── LICENSE
├── models
│   └── yolo_v8_nano_seg.onnx
├── notes.md
├── ort/
├── outputs/
├── README.md
├── src
│   ├── api
│   │   ├── handlers/
│   │   └── server.hpp
│   ├── db
│   │   ├── Dabase.hpp
│   │   ├── Databse.cpp
│   │   ├── postgres_connector.cpp
│   │   └── schema.sql
│   ├── inference
│   │   ├── metrology.cpp
│   │   ├── metrics_calculator.cpp
│   │   └── rock_detector.cpp
│   ├── main.cpp
│   ├── poc_main.cpp
│   └── utils
│       └── image_util.cpp
└── third_party/
```

## Getting Started

### Prerequisites

- [CMake](https://cmake.org/) (3.16 or higher)
- [OpenCV](https://opencv.org/) (4.5.0 or higher)
- [ONNX Runtime](https://onnxruntime.ai/) (1.7.0 or higher)
- [PostgreSQL](https://www.postgresql.org/) (13.0 or higher)

### Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/beaunix/RockPulseAPI.git
   ```

2. Create a build directory:

   ```bash
   mkdir build
   cd build
   ```

3. Configure the project:

   ```bash
   cmake ..
   ```

4. Build the project:

   ```bash
   cmake --build .
   ```

5. Run the tests:

   ```bash
   ctest
   ```

6. Run the application:

   ```bash
   ./RockPulseAPI
   ```

7. Open the application in your web browser:

   ```bash
   xdg-open http://localhost:8080
   ```

## Build and Start the Infrastructure (Backend + Postgres)  
docker-compose up --build

## License

[MIT License](https://github.com/beaunix/RockPulseAPI/blob/main/LICENSE)


## Author

Developed by Bryan Beau - 2026
AI Engineer & Software Architecture Student 