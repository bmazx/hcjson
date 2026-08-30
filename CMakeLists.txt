cmake_minimum_required(VERSION 3.11...4.2)

set(CMAKE_C_STANDARD 99)

project(hcjson)

add_executable(createJson hcjson.h examples/createJson.c)
target_include_directories(createJson PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(jsonFromString hcjson.h examples/jsonFromString.c)
target_include_directories(jsonFromString PRIVATE ${PROJECT_SOURCE_DIR})
