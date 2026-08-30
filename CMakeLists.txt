cmake_minimum_required(VERSION 3.11...4.2)

set(CMAKE_C_STANDARD 99)

project(hcjson)

add_executable(printjson hcjson.h examples/printjson.c)
target_include_directories(printjson PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(jsonstring hcjson.h examples/jsonstring.c)
target_include_directories(jsonstring PRIVATE ${PROJECT_SOURCE_DIR})
