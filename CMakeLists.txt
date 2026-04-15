cmake_minimum_required(VERSION 3.10)
project(RockPulseAPI)

set(CMAKE_CXX_STANDARD 17)

# fetch threads library really important for crow
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#  search for OpenCV
find_package(OpenCV REQUIRED)

#  search for libpqxx (PostgreSQL)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

#  ONNX Runtime
if(DEFINED ONNXRUNTIME_ROOT)
    set(ORT_DIR "${ONNXRUNTIME_ROOT}")
else()
    set(ORT_DIR "${CMAKE_SOURCE_DIR}/ort")
endif()

set(ORT_INCLUDE_DIR "${ORT_DIR}/include")
find_library(ORT_LIB onnxruntime PATHS "${ORT_DIR}/lib" REQUIRED)

# Add directories 
include_directories(
    ${OpenCV_INCLUDE_DIRS} 
    ${ORT_INCLUDE_DIR}
    ${PQXX_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party 
)

# R-Compile C++ source files in src/
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

#Excecute the API
add_executable(RockPulseAPI main.cpp ${SRC_FILES})
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/poc_script.cpp")

target_link_libraries(RockPulseAPI 
    ${OpenCV_LIBRARIES} 
    ${ORT_LIB} 
    ${PQXX_LIBRARIES}
    Threads::Threads
)

add_executable(RockPulsePoC src/poc_script.cpp)
target_link_libraries(RockPulsePoC 
    ${OpenCV_LIBRARIES} 
    ${ORT_LIB} 
    Threads::Threads
)
