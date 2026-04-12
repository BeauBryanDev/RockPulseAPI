FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    git \
    libopencv-dev \
    libpqxx-dev \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install ONNX Runtime C++ SDK
RUN wget https://github.com/microsoft/onnxruntime/releases/download/v1.21.0/onnxruntime-linux-x64-1.21.0.tgz \
    && tar -xzf onnxruntime-linux-x64-1.21.0.tgz \
    && mv onnxruntime-linux-x64-1.21.0 /opt/onnxruntime \
    && rm onnxruntime-linux-x64-1.21.0.tgz

COPY . .

RUN mkdir build && cd build && \
    cmake -DONNXRUNTIME_ROOT=/opt/onnxruntime .. && \
    make -j$(nproc)


FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /app

# Runtime OpenCV libs: core + imgcodecs + imgproc + dnn (for NMSBoxes)
RUN apt-get update && apt-get install -y \
    libopencv-core4.5d \
    libopencv-imgcodecs4.5d \
    libopencv-imgproc4.5d \
    libopencv-dnn4.5d \
    libpqxx-6.4 \
    libpq5 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/RockPulseAPI /app/RockPulseAPI
COPY --from=builder /opt/onnxruntime/lib/libonnxruntime.so* /usr/local/lib/
RUN ldconfig

COPY ./model /app/model

# Persistent output directory (bind-mounted from host)
RUN mkdir -p /app/outputs

EXPOSE 8080

CMD ["./RockPulseAPI"]
