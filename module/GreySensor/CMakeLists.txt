# CMakeLists.txt for GreySensor

target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB MODULE_GREYSENSOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
)

target_sources(xr PRIVATE ${MODULE_GREYSENSOR_SRC})
