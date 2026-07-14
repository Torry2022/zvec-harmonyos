cmake_minimum_required(VERSION 3.5.0)
project(zvec_ohos_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ZVEC_INCLUDE_DIR "" CACHE PATH "Directory containing zvec/c_api.h")
set(ZVEC_LIBRARY "" CACHE FILEPATH "Path to libzvec_c_api.so")

if(NOT EXISTS "${ZVEC_INCLUDE_DIR}/zvec/c_api.h")
    message(FATAL_ERROR "Set ZVEC_INCLUDE_DIR to the Zvec C API include directory")
endif()

if(NOT EXISTS "${ZVEC_LIBRARY}")
    message(FATAL_ERROR "Set ZVEC_LIBRARY to the built libzvec_c_api.so")
endif()

add_library(zvec_c_api SHARED IMPORTED GLOBAL)
set_target_properties(zvec_c_api PROPERTIES IMPORTED_LOCATION "${ZVEC_LIBRARY}")

add_library(zvec_smoke STATIC zvec_smoke.cpp)
target_include_directories(zvec_smoke PUBLIC "${CMAKE_CURRENT_LIST_DIR}" "${ZVEC_INCLUDE_DIR}")
target_link_libraries(zvec_smoke PUBLIC zvec_c_api)
