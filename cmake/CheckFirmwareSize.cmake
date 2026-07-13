if(NOT DEFINED FIRMWARE_BIN)
  message(FATAL_ERROR "FIRMWARE_BIN is required")
endif()

if(NOT DEFINED FIRMWARE_MAX_SIZE_BYTES)
  message(FATAL_ERROR "FIRMWARE_MAX_SIZE_BYTES is required")
endif()

if(NOT EXISTS "${FIRMWARE_BIN}")
  message(FATAL_ERROR "Firmware binary does not exist: ${FIRMWARE_BIN}")
endif()

file(SIZE "${FIRMWARE_BIN}" FIRMWARE_SIZE_BYTES)
if(FIRMWARE_SIZE_BYTES GREATER FIRMWARE_MAX_SIZE_BYTES)
  message(FATAL_ERROR
    "Firmware binary is ${FIRMWARE_SIZE_BYTES} bytes, exceeding the "
    "${FIRMWARE_MAX_SIZE_BYTES}-byte flash budget")
endif()

message(STATUS
  "Firmware binary size: ${FIRMWARE_SIZE_BYTES}/${FIRMWARE_MAX_SIZE_BYTES} bytes")
