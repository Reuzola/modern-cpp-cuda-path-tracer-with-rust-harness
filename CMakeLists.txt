cmake_minimum_required(VERSION 3.25)

project(PathTracer
   	VERSION 0.1.0
   	DESCRIPTION "Physically-based path tracer"
   	LANGUAGES CXX
)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pathtracer src/main.cpp)
target_include_directories(pathtracer PRIVATE include external)