cmake_minimum_required(VERSION 3.10)
project(RockPulseAPI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads  REQUIRED)
find_package(OpenCV   REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL        REQUIRED)
find_package(nlohmann_json 3 REQUIRED)

pkg_check_modules(PQXX REQUIRED libpqxx)

# ONNX Runtime — local dev uses ./ort/, Docker passes -DONNXRUNTIME_ROOT=...
if(DEFINED ONNXRUNTIME_ROOT)
    set(ORT_DIR "${ONNXRUNTIME_ROOT}")
else()
    set(ORT_DIR "${CMAKE_SOURCE_DIR}/ort")
endif()

set(ORT_INCLUDE_DIR "${ORT_DIR}/include")
find_library(ORT_LIB onnxruntime PATHS "${ORT_DIR}/lib" REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${ORT_INCLUDE_DIR}
    ${PQXX_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

# Collect all API sources, exclude the standalone PoC script
file(GLOB_RECURSE ALL_SRC "src/*.cpp")
list(REMOVE_ITEM ALL_SRC "${CMAKE_SOURCE_DIR}/src/poc_main.cpp")

add_executable(RockPulseAPI ${ALL_SRC})
target_link_libraries(RockPulseAPI PRIVATE
    ${OpenCV_LIBRARIES}
    ${ORT_LIB}
    ${PQXX_LIBRARIES}
    Threads::Threads
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# Confirm : pkg-config --libs opencv4 | grep objdetect 

add_executable(RockPulsePoC src/poc_main.cpp)
target_link_libraries(RockPulsePoC PRIVATE
    ${OpenCV_LIBRARIES}
    ${ORT_LIB}
    Threads::Threads
)
