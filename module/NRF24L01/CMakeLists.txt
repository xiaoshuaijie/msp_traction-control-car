# CMakeLists.txt for NRF24L01

target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB MODULE_NRF24L01_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

target_sources(xr PRIVATE ${MODULE_NRF24L01_SRC})
