# CMakeLists.txt for Tracking

target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB MODULE_TRACKING_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/*.cxx"
    "${CMAKE_CURRENT_LIST_DIR}/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
)

target_sources(xr PRIVATE ${MODULE_TRACKING_SRC})
