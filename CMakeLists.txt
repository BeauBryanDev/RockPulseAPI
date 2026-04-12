cmake_minimum_required(VERSION 3.10)
project(RockPulsePoC)

set(CMAKE_CXX_STANDARD 17)

# OpenCV (for image loading only)
find_package(OpenCV REQUIRED)

# ONNX Runtime C++ SDK
set(ORT_DIR "${CMAKE_SOURCE_DIR}/ort")
set(ORT_INCLUDE_DIR "${ORT_DIR}/include")
set(ORT_LIB "${ORT_DIR}/lib/libonnxruntime.so")

include_directories(${OpenCV_INCLUDE_DIRS} ${ORT_INCLUDE_DIR})

add_executable(RockPulsePoC main.cpp)
target_link_libraries(RockPulsePoC ${OpenCV_LIBRARIES} ${ORT_LIB})
