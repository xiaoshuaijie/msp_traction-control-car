# MPU6050 模块构建配置。

# 将当前目录公开给 xr 的使用者，使业务代码可以直接包含 MPU6050.hpp。
target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# 自动收集模块内的 C/C++ 实现文件；CONFIGURE_DEPENDS 使新增源文件时触发重新配置。
file(GLOB MODULE_MPU6050_SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/*.cxx"
  "${CMAKE_CURRENT_LIST_DIR}/*.cc"
  "${CMAKE_CURRENT_LIST_DIR}/*.c"
)

target_sources(xr PRIVATE ${MODULE_MPU6050_SRC})

# 为模块预留独立的 INTERFACE 依赖目标，避免把可选依赖直接写入 xr。
get_filename_component(_MODULE_DIRNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(_DEPS_TARGET "${_MODULE_DIRNAME}_deps")
add_library(${_DEPS_TARGET} INTERFACE)

# 如模块后续引入第三方库，可在此查找包并链接到 _DEPS_TARGET。
# find_package(YourPkg REQUIRED COMPONENTS a b c)

# target_link_libraries(${_DEPS_TARGET} INTERFACE
#   # YourLibA
#   # YourLibB
# )

# target_compile_definitions(${_DEPS_TARGET} INTERFACE
#   # FOO=1
# )
# target_compile_options(${_DEPS_TARGET} INTERFACE
#   # -Wall
# )

# 注册到全局模块依赖列表，由顶层构建脚本统一链接到最终固件目标。
set_property(GLOBAL APPEND PROPERTY XR_MODULE_DEPS ${_DEPS_TARGET})
