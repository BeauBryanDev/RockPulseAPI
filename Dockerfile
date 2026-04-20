# Build Stage 
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    curl \
    pkg-config \
    libopencv-dev \
    libpqxx-dev \
    libpq-dev \
    libssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Download ONNX Runtime C++ SDK v1.21.0
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.21.0/onnxruntime-linux-x64-1.21.0.tgz \
    && tar -xzf onnxruntime-linux-x64-1.21.0.tgz \
    && mv onnxruntime-linux-x64-1.21.0 /opt/onnxruntime \
    && rm onnxruntime-linux-x64-1.21.0.tgz

COPY . .

RUN cmake -S . -B build -DONNXRUNTIME_ROOT=/opt/onnxruntime \
    && cmake --build build --parallel $(nproc)


# Runtime Stage
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /app

RUN apt-get update && apt-get install -y \
    libopencv-dev \
    libpq5 \
    libpqxx-7 \
    libssl3 \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*


COPY --from=builder /app/build/RockPulseAPI     /app/RockPulseAPI
COPY --from=builder /opt/onnxruntime/lib/     /usr/local/lib/
COPY --from=builder /app/models                 /app/models
COPY --from=builder /app/config                 /app/config


ENV LD_LIBRARY_PATH=/usr/local/lib
RUN ldconfig


RUN mkdir -p /app/data/inputs /app/data/outputs /app/logs
   
EXPOSE 8080

CMD ["./RockPulseAPI"]
