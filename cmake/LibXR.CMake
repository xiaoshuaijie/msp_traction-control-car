set(LIBXR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libxr" CACHE PATH "Path to LibXR source tree")
set(MSPM0_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mspm0-sdk" CACHE PATH "Path to MSPM0 SDK root")

if(NOT EXISTS "${LIBXR_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "LibXR not found. Please set LIBXR_DIR correctly: ${LIBXR_DIR}")
endif()

set(LIBXR_SYSTEM None)
set(LIBXR_DRIVER mspm0)
set(LIBXR_NO_EIGEN True)

if(NOT EXISTS "${MSPM0_SDK_DIR}/source")
    message(FATAL_ERROR "MSPM0 SDK not found. Please set MSPM0_SDK_DIR correctly: ${MSPM0_SDK_DIR}")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
    set(XROBOT_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
endif()

set(SYSCFG_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sysconfig" CACHE PATH "Path to SysConfig generated output")

file(GLOB _syscfg_c_candidates "${SYSCFG_OUTPUT_DIR}/ti_msp*_config.c")
file(GLOB _syscfg_h_candidates "${SYSCFG_OUTPUT_DIR}/ti_msp*_config.h")
file(GLOB _syscfg_linker_candidates "${SYSCFG_OUTPUT_DIR}/*.lds")
set(_syscfg_opt_file "${SYSCFG_OUTPUT_DIR}/device.opt")

list(LENGTH _syscfg_c_candidates _syscfg_c_count)
if(_syscfg_c_count EQUAL 0)
    message(FATAL_ERROR "No SysConfig C source found in ${SYSCFG_OUTPUT_DIR} (expected ti_msp*_config.c)")
elseif(_syscfg_c_count GREATER 1)
    message(FATAL_ERROR "Multiple SysConfig C sources found in ${SYSCFG_OUTPUT_DIR}: ${_syscfg_c_candidates}")
endif()

list(LENGTH _syscfg_h_candidates _syscfg_h_count)
if(_syscfg_h_count EQUAL 0)
    message(FATAL_ERROR "No SysConfig header found in ${SYSCFG_OUTPUT_DIR} (expected ti_msp*_config.h)")
elseif(_syscfg_h_count GREATER 1)
    message(FATAL_ERROR "Multiple SysConfig headers found in ${SYSCFG_OUTPUT_DIR}: ${_syscfg_h_candidates}")
endif()

list(LENGTH _syscfg_linker_candidates _syscfg_linker_count)
if(_syscfg_linker_count EQUAL 0)
    message(FATAL_ERROR "No linker script (*.lds) found in ${SYSCFG_OUTPUT_DIR}")
elseif(_syscfg_linker_count GREATER 1)
    list(FILTER _syscfg_linker_candidates INCLUDE REGEX ".*/device_linker\\.lds$")
    list(LENGTH _syscfg_linker_candidates _preferred_linker_count)
    if(_preferred_linker_count EQUAL 1)
        list(GET _syscfg_linker_candidates 0 SYSCFG_LINKER_FILE)
    else()
        message(FATAL_ERROR "Multiple linker scripts found in ${SYSCFG_OUTPUT_DIR}: ${_syscfg_linker_candidates}")
    endif()
else()
    list(GET _syscfg_linker_candidates 0 SYSCFG_LINKER_FILE)
endif()

if(NOT DEFINED SYSCFG_LINKER_FILE)
    list(GET _syscfg_linker_candidates 0 SYSCFG_LINKER_FILE)
endif()

if(NOT EXISTS "${_syscfg_opt_file}")
    message(FATAL_ERROR "Required SysConfig options file missing: ${_syscfg_opt_file}")
endif()

list(GET _syscfg_c_candidates 0 SYSCFG_C_FILE)
list(GET _syscfg_h_candidates 0 SYSCFG_H_FILE)

set(MSPM0_STARTUP_FILE "${MSPM0_SDK_DIR}/examples/nortos/LP_MSPM0G3507/driverlib/empty_driverlib_library/gcc/startup_mspm0g350x_gcc.c"
    CACHE FILEPATH "MSPM0 startup source file")
if(NOT EXISTS "${MSPM0_STARTUP_FILE}")
    message(FATAL_ERROR "MSPM0 startup file not found: ${MSPM0_STARTUP_FILE}")
endif()

set(MSPM0_ARCH_COMPILE_OPTIONS
    -mcpu=cortex-m0plus
    -mthumb
    -mfloat-abi=soft)

set(MSPM0_SDK_INCLUDE_DIRS
    "${MSPM0_SDK_DIR}/source"
    "${MSPM0_SDK_DIR}/source/ti/devices/msp"
    "${MSPM0_SDK_DIR}/source/third_party/CMSIS/Core/Include"
    "${MSPM0_SDK_DIR}/source/ti/driverlib"
    "${MSPM0_SDK_DIR}/source/ti/driverlib/m0p")

set(MSPM0_COMMON_INCLUDE_DIRS
    "${SYSCFG_OUTPUT_DIR}"
    ${MSPM0_SDK_INCLUDE_DIRS})

set(MSPM0_COMMON_COMPILE_DEFINITIONS
    __MSPM0G3507__
    __USE_SYSCONFIG__)

set(MSPM0_DRIVERLIB_A "${MSPM0_SDK_DIR}/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a"
    CACHE FILEPATH "MSPM0 driverlib archive")
if(NOT EXISTS "${MSPM0_DRIVERLIB_A}")
    message(FATAL_ERROR "MSPM0 driverlib archive not found: ${MSPM0_DRIVERLIB_A}")
endif()

add_subdirectory("${LIBXR_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libxr")

if(TARGET xr)
    target_include_directories(xr PUBLIC
        ${MSPM0_COMMON_INCLUDE_DIRS})

    target_compile_options(xr PUBLIC
        ${MSPM0_ARCH_COMPILE_OPTIONS})

    target_compile_definitions(xr PUBLIC
        ${MSPM0_COMMON_COMPILE_DEFINITIONS})

    target_link_libraries(xr PUBLIC "${MSPM0_DRIVERLIB_A}")
endif()

if(TARGET xr)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _libxr_build_type)
    if(_libxr_build_type STREQUAL "debug")
        target_compile_options(xr PRIVATE -O0)
    endif()
endif()
