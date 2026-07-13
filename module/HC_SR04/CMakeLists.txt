# 使用 PUBLIC 公开本模块的头文件搜索路径，使 xr 自身及所有依赖 xr 的目标都能包含 HC_SR04 的头文件。
target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# 使用 PRIVATE 仅将 sr04.cpp 注册为 xr 的内部实现源文件，避免把实现源传播给依赖 xr 的其他目标。
target_sources(xr PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/sr04.cpp")
