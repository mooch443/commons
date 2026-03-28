cmake_minimum_required(VERSION 3.28)
project(commons_module_consumer_smoke LANGUAGES CXX)

set(COMMONS_CMAKE_DIR "" CACHE PATH "Directory containing commonsTargets.cmake")
if(COMMONS_CMAKE_DIR STREQUAL "")
    message(FATAL_ERROR "Set COMMONS_CMAKE_DIR to the directory containing commonsTargets.cmake.")
endif()

include("${COMMONS_CMAKE_DIR}/commonsTargets.cmake")

add_executable(commons_module_consumer_smoke main.cpp)
target_link_libraries(commons_module_consumer_smoke PRIVATE Commons::All)
