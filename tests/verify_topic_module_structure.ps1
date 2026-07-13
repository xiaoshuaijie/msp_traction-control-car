$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$encoderHeaderPath = Join-Path $repoRoot "module/encoder/encoder.hpp"
$encoderSourcePath = Join-Path $repoRoot "module/encoder/encoder.cpp"
$encoderCMakePath = Join-Path $repoRoot "module/encoder/CMakeLists.txt"
$nrfHeaderPath = Join-Path $repoRoot "module/NRF24L01/NRF24L01.hpp"
$nrfSourcePath = Join-Path $repoRoot "module/NRF24L01/NRF24L01.cpp"
$nrfBusHeaderPath = Join-Path $repoRoot "module/NRF24L01/nrf24l01_bus.hpp"
$nrfBusSourcePath = Join-Path $repoRoot "module/NRF24L01/nrf24l01_bus.cpp"
$nrfStateHeaderPath = Join-Path $repoRoot "module/NRF24L01/nrf24l01_state.hpp"
$nrfCMakePath = Join-Path $repoRoot "module/NRF24L01/CMakeLists.txt"
$legacyNrfHeaderPath = Join-Path $repoRoot "module/NRF24L01/nrf24l01.h"
$legacyNrfAppHeaderPath = Join-Path $repoRoot "module/NRF24L01/NRF24L01App.hpp"
$legacyNrfAppSourcePath = Join-Path $repoRoot "module/NRF24L01/NRF24L01App.cpp"
$rootCMakePath = Join-Path $repoRoot "CMakeLists.txt"

$failures = [System.Collections.Generic.List[string]]::new()

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        $failures.Add("Missing ${Description}: $Path")
    }
}

function Assert-Matches {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Description
    )

    if ($Content -notmatch $Pattern) {
        $failures.Add("Missing contract: $Description")
    }
}

function Assert-DoesNotMatch {
    param(
        [string]$Content,
        [string]$Pattern,
        [string]$Description
    )

    if ($Content -match $Pattern) {
        $failures.Add("Forbidden legacy contract remains: $Description")
    }
}

Assert-FileExists $encoderHeaderPath "Encoder declaration"
Assert-FileExists $encoderSourcePath "Encoder implementation"
Assert-FileExists $encoderCMakePath "Encoder CMake file"
Assert-FileExists $nrfHeaderPath "NRF24L01 Application declaration"
Assert-FileExists $nrfSourcePath "NRF24L01 Application implementation"
Assert-FileExists $nrfBusHeaderPath "NRF24L01 software SPI declaration"
Assert-FileExists $nrfBusSourcePath "NRF24L01 software SPI implementation"
Assert-FileExists $nrfStateHeaderPath "NRF24L01 pure state logic"
Assert-FileExists $nrfCMakePath "NRF24L01 CMake file"

$encoderHeader = if (Test-Path -LiteralPath $encoderHeaderPath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $encoderHeaderPath
} else { "" }
$encoderSource = if (Test-Path -LiteralPath $encoderSourcePath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $encoderSourcePath
} else { "" }
$encoderCMake = if (Test-Path -LiteralPath $encoderCMakePath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $encoderCMakePath
} else { "" }
$nrfHeader = if (Test-Path -LiteralPath $nrfHeaderPath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $nrfHeaderPath
} else { "" }
$nrfSource = if (Test-Path -LiteralPath $nrfSourcePath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $nrfSourcePath
} else { "" }
$nrfBusHeader = if (Test-Path -LiteralPath $nrfBusHeaderPath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $nrfBusHeaderPath
} else { "" }
$nrfBusSource = if (Test-Path -LiteralPath $nrfBusSourcePath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $nrfBusSourcePath
} else { "" }
$nrfStateHeader = if (Test-Path -LiteralPath $nrfStateHeaderPath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $nrfStateHeaderPath
} else { "" }
$nrfCMake = if (Test-Path -LiteralPath $nrfCMakePath) {
    Get-Content -Raw -Encoding UTF8 -LiteralPath $nrfCMakePath
} else { "" }
$rootCMake = Get-Content -Raw -Encoding UTF8 -LiteralPath $rootCMakePath

Assert-Matches $encoderHeader 'class\s+Encoder\s*:\s*public\s+LibXR::Application' `
    "Encoder inherits LibXR::Application"
Assert-Matches $encoderHeader 'struct\s+Config\s*\{[\s\S]*?topic_name\s*=\s*"encoder"\s*;' `
    'Config defaults topic_name to "encoder"'
Assert-Matches $encoderHeader 'struct\s+Config\s*\{[\s\S]*?publish_period_ms\s*=\s*5(?:U|u|UL|ul)?\s*;' `
    "Config defaults publish_period_ms to 5"
Assert-Matches $encoderHeader 'struct\s+Config\s*\{[\s\S]*?counts_per_rev\s*=\s*1024(?:\.0+)?(?:F|f)?\s*;' `
    "Config defaults counts_per_rev to 1024"

Assert-Matches $encoderHeader 'struct\s+Sample\s*\{[\s\S]*?std::array\s*<\s*(?:std::)?int32_t\s*,\s*kMotorCount\s*>\s+count' `
    "Sample stores four int32 counts"
Assert-Matches $encoderHeader 'struct\s+Sample\s*\{[\s\S]*?std::array\s*<\s*double\s*,\s*kMotorCount\s*>\s+angle_rad' `
    "Sample stores four double angles"
Assert-Matches $encoderHeader 'struct\s+Sample\s*\{[\s\S]*?std::array\s*<\s*float\s*,\s*kMotorCount\s*>\s+speed_rad_s' `
    "Sample stores four float speeds"
Assert-Matches $encoderHeader 'struct\s+Sample\s*\{[\s\S]*?(?:std::)?uint32_t\s+sample_time_ms' `
    "Sample stores sample_time_ms"
Assert-Matches $encoderHeader 'struct\s+Sample\s*\{[\s\S]*?(?:std::)?uint32_t\s+sequence' `
    "Sample stores sequence"

Assert-Matches $encoderHeader 'Encoder\s*\(\s*LibXR::HardwareContainer\s*&\s*\w+\s*,\s*LibXR::ApplicationManager\s*&\s*\w+\s*\)\s*;' `
    "Encoder has HardwareContainer/ApplicationManager constructor"
Assert-Matches $encoderHeader 'Encoder\s*\(\s*LibXR::HardwareContainer\s*&\s*\w+\s*,\s*LibXR::ApplicationManager\s*&\s*\w+\s*,\s*const\s+Config\s*&\s*\w+\s*\)\s*;' `
    "Encoder has Config constructor"
Assert-Matches $encoderHeader 'LibXR::Topic\s*&\s*SampleTopic\s*\(' `
    "Encoder exposes SampleTopic"
Assert-Matches $encoderHeader 'const\s+Sample\s*&\s*LatestSample\s*\(\s*\)\s*const' `
    "Encoder exposes LatestSample"
Assert-Matches $encoderHeader 'void\s+ResetAll\s*\(\s*\)\s*;' `
    "Encoder exposes ResetAll"
Assert-Matches $encoderHeader 'void\s+OnMonitor\s*\(\s*\)\s*override\s*;' `
    "Encoder overrides OnMonitor"
Assert-Matches $encoderHeader 'volatile\s+(?:std::)?uint32_t\s+count_' `
    "QuadratureDecoder stores the modular count as volatile uint32_t"
Assert-Matches $encoderHeader 'void\s+Shutdown\s*\(\s*\)\s*;' `
    "QuadratureDecoder exposes an interrupt shutdown path"
Assert-Matches $encoderHeader '~Encoder\s*\(\s*\)\s*override\s*;' `
    "Encoder destructor overrides Application destructor"
Assert-Matches $encoderHeader 'ApplicationManager::MonitorAll\s*\(\s*\)[\s\S]{0,80}LibXR' `
    "Encoder lifetime documentation covers ApplicationManager::MonitorAll"

Assert-Matches $encoderHeader 'QuadratureDecoder\s*\(\s*const\s+QuadratureDecoder\s*&\s*\)\s*=\s*delete\s*;' `
    "QuadratureDecoder deletes copy construction"
Assert-Matches $encoderHeader 'QuadratureDecoder\s*\(\s*QuadratureDecoder\s*&&\s*\)\s*=\s*delete\s*;' `
    "QuadratureDecoder deletes move construction"
Assert-Matches $encoderHeader 'operator=\s*\(\s*const\s+QuadratureDecoder\s*&\s*\)\s*=\s*delete\s*;' `
    "QuadratureDecoder deletes copy assignment"
Assert-Matches $encoderHeader 'operator=\s*\(\s*QuadratureDecoder\s*&&\s*\)\s*=\s*delete\s*;' `
    "QuadratureDecoder deletes move assignment"
Assert-Matches $encoderHeader 'Encoder\s*\(\s*const\s+Encoder\s*&\s*\)\s*=\s*delete\s*;' `
    "Encoder deletes copy construction"
Assert-Matches $encoderHeader 'Encoder\s*\(\s*Encoder\s*&&\s*\)\s*=\s*delete\s*;' `
    "Encoder deletes move construction"
Assert-Matches $encoderHeader 'operator=\s*\(\s*const\s+Encoder\s*&\s*\)\s*=\s*delete\s*;' `
    "Encoder deletes copy assignment"
Assert-Matches $encoderHeader 'operator=\s*\(\s*Encoder\s*&&\s*\)\s*=\s*delete\s*;' `
    "Encoder deletes move assignment"

Assert-DoesNotMatch $encoderHeader '(?<!~)\bEncoder\s*\(\s*\)' "default constructor"
Assert-DoesNotMatch $encoderHeader '\bGetCount\s*\(\s*MotorId\b' `
    "old Encoder GetCount aggregate API"
Assert-DoesNotMatch $encoderHeader '\bGetAngle\s*\(\s*MotorId\b' `
    "old Encoder GetAngle aggregate API"
Assert-DoesNotMatch $encoderHeader '\bGetSpeed\s*\(\s*MotorId\b' `
    "old Encoder GetSpeed aggregate API"

Assert-Matches $encoderSource 'LibXR::Topic::CreateTopic\s*<\s*Sample\s*>' `
    "Encoder creates its Sample topic"
Assert-Matches $encoderSource 'app\.Register\s*\(\s*\*this\s*\)' `
    "Encoder registers with ApplicationManager"
Assert-Matches $encoderSource 'hw\.Register\s*\(' `
    "Encoder registers with HardwareContainer"
Assert-Matches $encoderSource 'EncoderMath::BuildSample\s*\(' `
    "Encoder uses EncoderMath::BuildSample"
Assert-Matches $encoderSource '(?:sample_topic_|topic_)\.Publish\s*\(' `
    "Encoder publishes Sample topic data"
Assert-Matches $encoderSource 'class\s+InterruptGuard[\s\S]*?__get_PRIMASK\s*\([\s\S]*?__disable_irq\s*\([\s\S]*?__set_PRIMASK\s*\(' `
    "interrupt guard saves, disables, and restores PRIMASK"
Assert-Matches $encoderSource 'Encoder::ReadCounts\s*\(\s*\)\s*const\s*\{[\s\S]*?InterruptGuard\s+\w+\s*;[\s\S]*?for\s*\(' `
    "ReadCounts snapshots all four wheels inside one interrupt guard"
Assert-Matches $encoderSource 'Encoder::ResetAll\s*\(\s*\)\s*\{[\s\S]*?InterruptGuard\s+\w+\s*;[\s\S]*?for\s*\(' `
    "ResetAll resets all four wheels inside one interrupt guard"
Assert-Matches $encoderSource 'std::bit_cast\s*<\s*(?:std::)?int32_t\s*>\s*\(' `
    "QuadratureDecoder converts modular counts with std::bit_cast"
Assert-Matches $encoderSource 'count_\s*=\s*(?:self->)?count_\s*\+\s*1U\s*;' `
    "OnEdge increments the modular unsigned count"
Assert-Matches $encoderSource 'count_\s*=\s*(?:self->)?count_\s*-\s*1U\s*;' `
    "OnEdge decrements the modular unsigned count"
Assert-Matches $encoderSource 'QuadratureDecoder::Shutdown\s*\(\s*\)\s*\{[\s\S]*?a_\.DisableInterrupt\s*\(\s*\)[\s\S]*?b_\.DisableInterrupt\s*\(\s*\)' `
    "QuadratureDecoder shutdown disables both phase interrupts"
Assert-Matches $encoderSource 'Encoder::~Encoder\s*\(\s*\)\s*\{[\s\S]*?for\s*\([\s\S]*?\.Shutdown\s*\(\s*\)' `
    "Encoder destructor shuts down all four decoders"
Assert-DoesNotMatch $encoderSource '\bEncoder::Init\s*\(' `
    "old Encoder Init aggregate API"

Assert-Matches $encoderCMake 'target_sources\s*\(\s*xr\s+PRIVATE[\s\S]*encoder\.cpp' `
    "Encoder CMake compiles encoder.cpp into xr"
Assert-Matches $rootCMake 'add_subdirectory\s*\(\s*"?\$\{CMAKE_SOURCE_DIR\}/module/encoder"?\s*\)' `
    "root CMake adds module/encoder"

Assert-Matches $nrfHeader 'class\s+NRF24L01\s*:\s*public\s+LibXR::Application' `
    "NRF24L01 inherits LibXR::Application"
Assert-Matches $nrfHeader 'struct\s+Config\s*\{[\s\S]*?address\s*=\s*\{\s*0x11\s*,\s*0x22\s*,\s*0x33\s*,\s*0x44\s*,\s*0x55\s*\}' `
    "NRF24L01 Config defaults its five-byte address"
Assert-Matches $nrfHeader 'struct\s+Config\s*\{[\s\S]*?channel\s*=\s*2(?:U|u)?\s*;' `
    "NRF24L01 Config defaults channel to 2"
Assert-Matches $nrfHeader 'data_rate\s*=\s*DataRate::MBPS_2' `
    "NRF24L01 Config defaults data rate to 2 Mbps"
Assert-Matches $nrfHeader 'output_power\s*=\s*OutputPower::DBM_0' `
    "NRF24L01 Config defaults output power to 0 dBm"
Assert-Matches $nrfHeader 'retry_delay_us\s*=\s*250(?:U|u)?\s*;' `
    "NRF24L01 Config defaults retry delay to 250 us"
Assert-Matches $nrfHeader 'retry_count\s*=\s*3(?:U|u)?\s*;' `
    "NRF24L01 Config defaults retry count to 3"
Assert-Matches $nrfHeader 'payload_width\s*=\s*32(?:U|u)?\s*;' `
    "NRF24L01 Config defaults fixed payload width to 32"
Assert-Matches $nrfHeader 'tx_topic_name\s*=\s*"nrf24l01_tx"' `
    "NRF24L01 Config defaults the TX topic name"
Assert-Matches $nrfHeader 'rx_topic_name\s*=\s*"nrf24l01_rx"' `
    "NRF24L01 Config defaults the RX topic name"
Assert-Matches $nrfHeader 'status_topic_name\s*=\s*"nrf24l01_status"' `
    "NRF24L01 Config defaults the status topic name"
Assert-Matches $nrfHeader 'rx_poll_period_ms\s*=\s*10(?:U|u)?\s*;' `
    "NRF24L01 Config defaults RX polling to 10 ms"
Assert-Matches $nrfHeader 'status_period_ms\s*=\s*100(?:U|u)?\s*;' `
    "NRF24L01 Config defaults status heartbeat to 100 ms"
Assert-Matches $nrfHeader 'recovery_period_ms\s*=\s*100(?:U|u)?\s*;' `
    "NRF24L01 Config defaults recovery to 100 ms"
Assert-Matches $nrfHeader 'tx_timeout_ms\s*=\s*100(?:U|u)?\s*;' `
    "NRF24L01 Config defaults TX timeout to 100 ms"

Assert-Matches $nrfHeader 'struct\s+TxRequest\s*\{[\s\S]*?request_id[\s\S]*?array\s*<\s*(?:std::)?uint8_t\s*,\s*5\s*>\s+target_address[\s\S]*?array\s*<\s*(?:std::)?uint8_t\s*,\s*32\s*>\s+payload' `
    "TxRequest carries request id, target address, and fixed payload"
Assert-Matches $nrfHeader 'struct\s+RxPacket\s*\{[\s\S]*?array\s*<\s*(?:std::)?uint8_t\s*,\s*32\s*>\s+payload[\s\S]*?pipe[\s\S]*?received_at_ms[\s\S]*?sequence' `
    "RxPacket carries payload metadata and sequence"
Assert-Matches $nrfHeader 'enum\s+class\s+State[\s\S]*?INITIALIZING[\s\S]*?RECEIVE[\s\S]*?TRANSMITTING[\s\S]*?RECOVERING' `
    "Status exposes all NRF24L01 runtime states"
Assert-Matches $nrfHeader 'enum\s+class\s+TxResult[\s\S]*?NONE[\s\S]*?SUCCESS[\s\S]*?MAX_RETRIES[\s\S]*?INVALID_STATUS[\s\S]*?TIMEOUT[\s\S]*?DEVICE_ERROR' `
    "Status exposes all NRF24L01 TX outcomes"
Assert-Matches $nrfHeader 'struct\s+Status\s*\{[\s\S]*?state[\s\S]*?tx_result[\s\S]*?raw_status[\s\S]*?request_id[\s\S]*?tx_success_count[\s\S]*?tx_failure_count[\s\S]*?rx_count[\s\S]*?sequence' `
    "Status carries state, result, raw status, counters, and sequence"

Assert-Matches $nrfHeader 'NRF24L01\s*\(\s*LibXR::HardwareContainer\s*&\s*\w+\s*,\s*LibXR::ApplicationManager\s*&\s*\w+\s*\)\s*;' `
    "NRF24L01 has HardwareContainer/ApplicationManager constructor"
Assert-Matches $nrfHeader 'NRF24L01\s*\(\s*LibXR::HardwareContainer\s*&\s*\w+\s*,\s*LibXR::ApplicationManager\s*&\s*\w+\s*,\s*const\s+Config\s*&\s*\w+\s*\)\s*;' `
    "NRF24L01 has Config constructor"
Assert-Matches $nrfHeader 'NRF24L01\s*\(\s*const\s+NRF24L01\s*&\s*\)\s*=\s*delete\s*;' `
    "NRF24L01 deletes copy construction"
Assert-Matches $nrfHeader 'NRF24L01\s*\(\s*NRF24L01\s*&&\s*\)\s*=\s*delete\s*;' `
    "NRF24L01 deletes move construction"
Assert-Matches $nrfHeader 'operator=\s*\(\s*const\s+NRF24L01\s*&\s*\)\s*=\s*delete\s*;' `
    "NRF24L01 deletes copy assignment"
Assert-Matches $nrfHeader 'operator=\s*\(\s*NRF24L01\s*&&\s*\)\s*=\s*delete\s*;' `
    "NRF24L01 deletes move assignment"
Assert-Matches $nrfHeader '~NRF24L01\s*\(\s*\)\s*override\s*;' `
    "NRF24L01 destructor overrides Application destructor"
Assert-Matches $nrfHeader 'LibXR::Topic\s*&\s*TxTopic\s*\(' "NRF24L01 exposes TxTopic"
Assert-Matches $nrfHeader 'LibXR::Topic\s*&\s*RxTopic\s*\(' "NRF24L01 exposes RxTopic"
Assert-Matches $nrfHeader 'LibXR::Topic\s*&\s*StatusTopic\s*\(' "NRF24L01 exposes StatusTopic"
Assert-Matches $nrfHeader 'const\s+Status\s*&\s*LatestStatus\s*\(\s*\)\s*const' `
    "NRF24L01 exposes LatestStatus"
Assert-Matches $nrfHeader 'void\s+OnMonitor\s*\(\s*\)\s*override\s*;' `
    "NRF24L01 overrides OnMonitor"
Assert-Matches $nrfHeader '\u751F\u547D\u5468\u671F\u5FC5\u987B\u8986\u76D6\u6240\u6709[\s\S]*ApplicationManager::MonitorAll' `
    "NRF24L01 lifetime documentation covers ApplicationManager::MonitorAll"
Assert-Matches $nrfHeader '\u5355\u5728\u9014[\s\S]*?RECEIVE[\s\S]*?\u7EC8\u6001[\s\S]*?Status' `
    "NRF24L01 documents when callers may publish the next TX request"
Assert-Matches $nrfHeader 'TRANSMITTING[\s\S]*?ASyncSubscriber[\s\S]*?IDLE[\s\S]*?\u9759\u9ED8\u4E22\u5F03[\s\S]*?\u65E0\u6CD5[\s\S]*?\u8BA1\u6570' `
    "NRF24L01 documents unobservable TX drops while transmitting"
Assert-Matches $nrfHeader 'RECOVERING[\s\S]*?\u6700\u591A\u6355\u83B7\u7B2C\u4E00\u6761\u65B0\u8BF7\u6C42[\s\S]*?RECEIVE' `
    "NRF24L01 documents the one-slot recovery request behavior"
Assert-Matches $nrfHeader 'CompleteTransmission\s*\([\s\S]*?TxOutcome\s+outcome\s*,[\s\S]*?TerminalTransition\s+transition\s*,\s*uint32_t\s+now_ms\s*\)' `
    "NRF24L01 terminal handling accepts the pure terminal transition"

Assert-Matches $nrfSource 'tx_topic_\s*\(\s*LibXR::Topic::CreateTopic\s*<\s*TxRequest\s*>[\s\S]*?tx_subscriber_\s*\(\s*tx_topic_\s*\)[\s\S]*?rx_topic_\s*\(\s*LibXR::Topic::CreateTopic\s*<\s*RxPacket\s*>[\s\S]*?status_topic_\s*\(\s*LibXR::Topic::CreateTopic\s*<\s*Status\s*>' `
    "NRF24L01 constructs TX topic/subscriber before RX and status topics"
Assert-Matches $nrfSource 'tx_subscriber_\.StartWaiting\s*\(\s*\)' `
    "NRF24L01 starts waiting for its first TX request"
Assert-Matches $nrfSource 'app\.Register\s*\(\s*\*this\s*\)' `
    "NRF24L01 registers with ApplicationManager"
Assert-Matches $nrfSource 'hw\.Register\s*\(' "NRF24L01 registers with HardwareContainer"
Assert-Matches $nrfSource 'Nrf24l01State::EvaluateTx\s*\(\s*raw_status\s*,\s*elapsed_ms\s*,\s*config_\.tx_timeout_ms\s*\)' `
    "NRF24L01 passes its configured timeout to TX evaluation"
Assert-Matches $nrfStateHeader 'TerminalTransition\s+ResolveTerminalTransition\s*\(\s*TxDecision\s+decision\s*\)' `
    "NRF24L01 state logic exposes the terminal transition reducer"
Assert-Matches $nrfStateHeader 'bool\s+RecoveryElapsed\s*\(' `
    "NRF24L01 state logic exposes wrap-safe recovery timing"
Assert-Matches $nrfSource 'Nrf24l01State::RecoveryElapsed\s*\(\s*now_ms\s*,\s*state_changed_ms_\s*,\s*config_\.recovery_period_ms\s*\)' `
    "RECOVERING uses the tested wrap-safe recovery helper"
Assert-Matches $nrfSource 'Nrf24l01State::ResolveTerminalTransition\s*\(\s*decision\s*\)' `
    "MonitorTransmission uses the tested terminal transition reducer"
Assert-DoesNotMatch $nrfSource 'ASSERT\s*\(\s*config_\.tx_timeout_ms\s*==' `
    "hard-coded NRF24L01 TX timeout assertion"
Assert-Matches $nrfSource 'ASSERT\s*\(\s*config_\.tx_timeout_ms\s*>\s*0(?:U|u)?\s*\)' `
    "NRF24L01 rejects a zero TX timeout"
Assert-Matches $nrfSource 'tx_subscriber_\.Available\s*\(\s*\)[\s\S]*?tx_subscriber_\.GetData\s*\(\s*\)' `
    "NRF24L01 consumes an available TX request"
Assert-Matches $nrfSource 'if\s*\(\s*!transition\.publish_status\s*\)[\s\S]*?return\s*;' `
    "pending TX returns without publishing a terminal status"
Assert-Matches $nrfSource 'CompleteTransmission\s*\(\s*decision\.outcome\s*,\s*transition\s*,\s*now_ms\s*\)' `
    "terminal TX forwards the reducer output exactly once"
Assert-Matches $nrfSource 'transition\.rearm_subscriber[\s\S]*?tx_subscriber_\.StartWaiting\s*\(\s*\)' `
    "terminal subscriber rearming is controlled by the reducer"
Assert-Matches $nrfSource 'TerminalState::kReceive[\s\S]*?EnterReceive\s*\(\s*now_ms\s*\)' `
    "successful reducer transition returns to receive mode"
Assert-Matches $nrfSource 'transition\.requires_reinitialize[\s\S]*?EnterRecovery\s*\(\s*result\s*,\s*now_ms\s*\)' `
    "failed reducer transitions enter reinitialization recovery"

$completeTransmission = [regex]::Match(
    $nrfSource,
    'void\s+NRF24L01::CompleteTransmission[\s\S]*?(?=void\s+NRF24L01::EnterReceive)')
if (-not $completeTransmission.Success) {
    $failures.Add("Missing contract: NRF24L01 CompleteTransmission implementation")
} else {
    Assert-DoesNotMatch $completeTransmission.Value 'PublishStatus\s*\(' `
        "duplicate status publication before EnterRecovery"
}

Assert-Matches $nrfBusSource 'for\s*\([^)]*<\s*8(?:U|u)?\s*;' `
    "software SPI swaps exactly eight bits"
Assert-Matches $nrfBusSource 'NRF24L01_RF_SETUP[\s\S]*?0x0E' `
    "bus configures 2 Mbps and 0 dBm as RF_SETUP 0x0E"
Assert-Matches $nrfBusSource 'NRF24L01_SETUP_RETR[\s\S]*?retry_delay_us[\s\S]*?retry_count' `
    "bus derives SETUP_RETR from retry delay and count"
Assert-Matches $nrfBusSource 'NRF24L01_RX_PW_P0[\s\S]*?payload_width' `
    "bus configures the fixed pipe-zero payload width"
Assert-DoesNotMatch ($nrfSource + $nrfBusSource) '\bwhile\s*\(' `
    "blocking while loop in NRF24L01 module"
Assert-DoesNotMatch ($nrfSource + $nrfBusSource) 'Thread::Sleep\s*\(' `
    "blocking Thread::Sleep in NRF24L01 module"

Assert-Matches $nrfCMake 'file\s*\(\s*GLOB\s+MODULE_NRF24L01_SRC[\s\S]*?\*\.cpp' `
    "NRF24L01 CMake globs C++ sources"
Assert-Matches $nrfCMake 'target_include_directories\s*\(\s*xr\s+PUBLIC\s+\$\{CMAKE_CURRENT_LIST_DIR\}\s*\)' `
    "NRF24L01 CMake exports its include directory"
Assert-Matches $rootCMake 'add_subdirectory\s*\(\s*"?\$\{CMAKE_SOURCE_DIR\}/module/NRF24L01"?\s*\)' `
    "root CMake adds module/NRF24L01"

if (Test-Path -LiteralPath $legacyNrfHeaderPath -PathType Leaf) {
    $failures.Add("Forbidden legacy NRF24L01 C API header remains: $legacyNrfHeaderPath")
}
if (Test-Path -LiteralPath $legacyNrfAppHeaderPath -PathType Leaf) {
    $failures.Add("Forbidden legacy NRF24L01App header remains: $legacyNrfAppHeaderPath")
}
if (Test-Path -LiteralPath $legacyNrfAppSourcePath -PathType Leaf) {
    $failures.Add("Forbidden legacy NRF24L01App implementation remains: $legacyNrfAppSourcePath")
}
Assert-DoesNotMatch ($nrfHeader + $nrfSource + $nrfBusHeader + $nrfBusSource) `
    '\bNRF24L01_(?:Init|Send|Receive|ReadReg|WriteReg|TxAddress|RxPacket)\b' `
    "legacy NRF24L01 global C API symbol"

if ($failures.Count -ne 0) {
    Write-Host "Topic module structure verification FAILED ($($failures.Count) issue(s)):" -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "  - $failure" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Topic module structure verification passed." -ForegroundColor Green
