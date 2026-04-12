cmake_minimum_required(VERSION 3.10)
project(RockPulseAPI)

set(CMAKE_CXX_STANDARD 17)

# OpenCV (image I/O, preprocessing, NMS utilities)
find_package(OpenCV REQUIRED)

# ONNX Runtime: use -D ONNXRUNTIME_ROOT=/opt/onnxruntime (Docker)
# or falls back to ./ort (local dev)
if(DEFINED ONNXRUNTIME_ROOT)
    set(ORT_DIR "${ONNXRUNTIME_ROOT}")
else()
    set(ORT_DIR "${CMAKE_SOURCE_DIR}/ort")
endif()

set(ORT_INCLUDE_DIR "${ORT_DIR}/include")
set(ORT_LIB        "${ORT_DIR}/lib/libonnxruntime.so")

include_directories(${OpenCV_INCLUDE_DIRS} ${ORT_INCLUDE_DIR})

add_executable(RockPulseAPI main.cpp)
target_link_libraries(RockPulseAPI ${OpenCV_LIBRARIES} ${ORT_LIB})
