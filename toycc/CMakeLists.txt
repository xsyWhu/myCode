
cmake_minimum_required(VERSION 3.14)
project(ToyCCompiler)
set(CMAKE_CXX_STANDARD 20)
include_directories(include)
file(GLOB SOURCES "src/*.cpp")
add_executable(toyc main.cpp ${SOURCES})
