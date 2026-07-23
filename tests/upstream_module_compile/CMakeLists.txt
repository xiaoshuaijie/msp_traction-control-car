cmake_minimum_required(VERSION 3.20)
project(upstream_module_compile LANGUAGES C CXX)

if(NOT DEFINED LIBXR_ROOT)
  message(FATAL_ERROR "LIBXR_ROOT must point to a clean upstream LibXR checkout")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(LIBXR_SYSTEM none CACHE STRING "" FORCE)
set(LIBXR_STATIC_BUILD ON CACHE BOOL "" FORCE)
add_subdirectory(${LIBXR_ROOT} libxr)

add_executable(upstream_module_compile main.cpp)
target_include_directories(upstream_module_compile PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../Modules/NRF24L01
    ${CMAKE_CURRENT_LIST_DIR}/../../Modules/MPU6050)
target_link_libraries(upstream_module_compile PRIVATE xr)
