# 将当前目录作为 xr 的公共头文件搜索路径，使固件目标及其他 xr 使用者能够包含
# encoder.hpp；encoder_math.hpp 为头文件内联实现，也通过同一路径对外可见。
target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# 编码器硬件接入与 Topic 发布实现在 encoder.cpp 中，随 LibXR 的 xr 静态库统一编译。
# 头文件无需列入 target_sources：CMake 会通过 encoder.cpp 的 include 依赖自动跟踪它们。
target_sources(xr PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/encoder.cpp"
)
