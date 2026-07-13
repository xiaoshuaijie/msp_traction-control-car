$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

function Read-RequiredFile {
  param([string]$Path, [string]$MissingMessage)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw $MissingMessage
  }
  return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path
}

function Resolve-RequiredFile {
  param([string[]]$Paths, [string]$MissingMessage)

  foreach ($path in $Paths) {
    if (Test-Path -LiteralPath $path) {
      return $path
    }
  }
  throw $MissingMessage
}

function Assert-Contains {
  param([string]$Content, [string]$Expected, [string]$Message)

  if (-not $Content.Contains($Expected)) {
    throw $Message
  }
}

function Assert-NotContains {
  param([string]$Content, [string]$Unexpected, [string]$Message)

  if ($Content.Contains($Unexpected)) {
    throw $Message
  }
}

function Assert-Matches {
  param([string]$Content, [string]$Pattern, [string]$Message)

  if ($Content -notmatch $Pattern) {
    throw $Message
  }
}

function Assert-NotMatches {
  param([string]$Content, [string]$Pattern, [string]$Message)

  if ($Content -match $Pattern) {
    throw $Message
  }
}

$nrfHeaderPath = Join-Path $root 'module/NRF24L01/NRF24L01.hpp'
$nrfSourcePath = Resolve-RequiredFile @(
  (Join-Path $root 'module/NRF24L01/NRF24L01.cpp'),
  (Join-Path $root 'module/NRF24L01/nrf24l01.cpp')
) 'Missing NRF24L01 Application implementation.'
$nrfBusSourcePath = Join-Path $root 'module/NRF24L01/nrf24l01_bus.cpp'
$nrfCMakePath = Join-Path $root 'module/NRF24L01/CMakeLists.txt'

$nrfHeader = Read-RequiredFile $nrfHeaderPath `
  'Missing NRF24L01 Application header.'
$nrfSource = Read-RequiredFile $nrfSourcePath `
  'Missing NRF24L01 Application implementation.'
$nrfBusSource = Read-RequiredFile $nrfBusSourcePath `
  'Missing NRF24L01 software SPI implementation.'
$nrfCMake = Read-RequiredFile $nrfCMakePath `
  'Missing NRF24L01 CMake configuration.'

Assert-Contains $nrfHeader 'class NRF24L01 : public LibXR::Application' `
  'NRF24L01 must be managed by ApplicationManager.'
Assert-Contains $nrfHeader 'tx_topic_name = "nrf24l01_tx";' `
  'NRF24L01 must default its TX topic name.'
Assert-Contains $nrfHeader 'rx_topic_name = "nrf24l01_rx";' `
  'NRF24L01 must default its RX topic name.'
Assert-Contains $nrfHeader 'status_topic_name = "nrf24l01_status";' `
  'NRF24L01 must default its status topic name.'
Assert-Contains $nrfHeader 'void OnMonitor() override;' `
  'NRF24L01 must expose its Application monitor callback.'
Assert-Matches $nrfSource `
  'Nrf24l01State::EvaluateTx\s*\(\s*raw_status\s*,\s*elapsed_ms\s*,\s*config_\.tx_timeout_ms\s*\)' `
  'NRF24L01 must evaluate TX state without blocking.'
Assert-Contains $nrfCMake '"${CMAKE_CURRENT_LIST_DIR}/*.cpp"' `
  'NRF24L01 CMake must compile C++ sources.'

$nrfModuleContent = $nrfHeader + $nrfSource + $nrfBusSource + $nrfCMake
Assert-NotContains $nrfModuleContent 'NRF24L01App' `
  'Legacy NRF24L01App adapter must not remain.'
Assert-NotContains $nrfModuleContent 'NRF24L01_Init' `
  'Legacy NRF24L01_Init C API must not remain.'
Assert-NotContains $nrfModuleContent 'NRF24L01_Receive' `
  'Legacy NRF24L01_Receive C API must not remain.'
Assert-NotMatches $nrfModuleContent '\bwhile\s*\(' `
  'NRF24L01 module must not use blocking while loops.'
Assert-NotMatches $nrfModuleContent 'Thread::Sleep\s*\(' `
  'NRF24L01 module must not sleep in monitor or bus code.'

$xrobotHeaderPath = Join-Path $root 'xrobot_main.hpp'
$appMainPath = Join-Path $root 'src/app_main.cpp'
$controlTopicLogicPath = Join-Path $root 'src/control_topic_logic.hpp'
$carControlHeaderPath = Join-Path $root 'src/car_control_support.hpp'
$carControlSourcePath = Join-Path $root 'src/car_control_support.cpp'
$rootCMakePath = Join-Path $root 'CMakeLists.txt'
$xrobotHeader = Read-RequiredFile $xrobotHeaderPath `
  'Missing XRobotModules composition header.'
$appMain = Read-RequiredFile $appMainPath `
  'Missing app_main integration source.'
$controlTopicLogic = Read-RequiredFile $controlTopicLogicPath `
  'Missing control topic pure-logic header.'
$carControlHeader = Read-RequiredFile $carControlHeaderPath `
  'Missing car control support header.'
$carControlSource = Read-RequiredFile $carControlSourcePath `
  'Missing car control support implementation.'
$rootCMake = Read-RequiredFile $rootCMakePath 'Missing root CMake configuration.'
$carControl = $carControlHeader + $carControlSource

Assert-Contains $appMain '#include "car_control_support.hpp"' `
  'app_main must consume the extracted car control support API.'
Assert-Contains $rootCMake '"${CMAKE_SOURCE_DIR}/src/car_control_support.cpp"' `
  'CMake APP_SOURCES must compile the car control support implementation.'
Assert-NotMatches $appMain 'float\s+ResolveTargetSpeed\s*\(' `
  'app_main must keep control helpers in car_control_support.'

Assert-Contains $xrobotHeader 'class XRobotModules' `
  'xrobot_main.hpp must define the XRobotModules composition class.'
Assert-Contains $xrobotHeader 'struct Config' `
  'XRobotModules must centralize module configuration.'
Assert-Contains $xrobotHeader '#include "sr04.h"' `
  'XRobotModules must include the HC-SR04 application contract.'
Assert-Contains $xrobotHeader 'kUltrasonicTopicName = "hc_sr04";' `
  'XRobotModules must centralize the HC-SR04 topic name.'
Assert-Contains $xrobotHeader 'HC_SR04::Config ultrasonic{};' `
  'XRobotModules Config must expose the HC-SR04 configuration.'
Assert-Contains $xrobotHeader 'XRobotModules(LibXR::HardwareContainer& hw,' `
  'XRobotModules must accept the shared HardwareContainer.'
Assert-Contains $xrobotHeader 'LibXR::ApplicationManager& app' `
  'XRobotModules must accept the shared ApplicationManager.'
Assert-Matches $xrobotHeader `
  'XRobotModules\s*\(\s*LibXR::HardwareContainer&\s+hw\s*,\s*LibXR::ApplicationManager&\s+app\s*,\s*const\s+HC_SR04::Resources&\s+ultrasonic_resources\s*\)' `
  'XRobotModules must require HC-SR04 board resources in its default-config constructor.'
Assert-Matches $xrobotHeader `
  'XRobotModules\s*\(\s*LibXR::HardwareContainer&\s+hw\s*,\s*LibXR::ApplicationManager&\s+app\s*,\s*const\s+HC_SR04::Resources&\s+ultrasonic_resources\s*,\s*const\s+Config&\s+config\s*\)' `
  'XRobotModules must require HC-SR04 board resources in its explicit-config constructor.'
Assert-Contains $xrobotHeader 'const Config& config' `
  'XRobotModules must expose an explicit Config constructor.'
Assert-NotContains $xrobotHeader 'BlinkLED' `
  'XRobotModules must not assemble the legacy BlinkLED module.'
Assert-NotContains $xrobotHeader 'JY61P' `
  'XRobotModules must not assemble the legacy JY61P module.'
Assert-NotMatches $xrobotHeader '\bApplicationManager\s+[A-Za-z_][A-Za-z0-9_]*\s*[;{(]' `
  'XRobotModules must not create a private ApplicationManager.'
Assert-NotMatches $xrobotHeader '\bwhile\s*\(' `
  'XRobotModules must not own an execution loop.'
Assert-NotMatches $xrobotHeader 'Thread::Sleep\s*\(' `
  'XRobotModules must not sleep.'
Assert-NotContains $xrobotHeader 'XRobotMain' `
  'The legacy XRobotMain function must be removed.'
Assert-NotContains $xrobotHeader 'MonitorAll' `
  'XRobotModules must not drive ApplicationManager itself.'

Assert-Contains $xrobotHeader 'kGreySensorAliases' `
  'XRobotModules must expose the eight grey-sensor GPIO aliases.'
Assert-Contains $xrobotHeader 'kImuI2cAliases' `
  'XRobotModules must expose the MPU6050 I2C aliases.'
Assert-Contains $xrobotHeader '"i2c_mpu6050", "imu", "i2c2"' `
  'XRobotModules must preserve all MPU6050 I2C lookup aliases.'
Assert-Contains $xrobotHeader 'kNrfGpioAliases' `
  'XRobotModules must expose the five NRF24L01 GPIO aliases.'
Assert-Contains $xrobotHeader '"nrf24l01_ce", "nrf24l01_csn", "nrf24l01_sck",' `
  'XRobotModules NRF aliases must match NRF24L01::Config defaults.'
Assert-Contains $xrobotHeader '"nrf24l01_mosi", "nrf24l01_miso"' `
  'XRobotModules NRF aliases must include MOSI and MISO.'
Assert-Contains $xrobotHeader 'grey_publish_period_ms = 10U;' `
  'XRobotModules must default GreySensor publication to 10 ms.'
Assert-Contains $xrobotHeader 'publish_period_ms = 5U' `
  'XRobotModules must default Encoder publication to 5 ms.'
Assert-Contains $xrobotHeader 'counts_per_rev = 1024.0F' `
  'XRobotModules must default Encoder counts per revolution to 1024.'
Assert-Contains $xrobotHeader 'ultrasonic.topic_name = kUltrasonicTopicName;' `
  'XRobotModules must bind HC-SR04 to its centralized topic name.'

Assert-Matches $xrobotHeader `
  '(?s)GreySensor\s+grey_sensor_;.*Tracking\s+tracking_;.*MPU6050\s+imu_;.*Module::Encoder\s+encoder_;.*NRF24L01\s+radio_;' `
  'XRobotModules members must follow GreySensor -> Tracking -> MPU6050 -> Encoder -> NRF24L01 dependency order.'
Assert-Matches $xrobotHeader `
  '(?s):\s*grey_sensor_\s*\(.*?\),\s*tracking_\s*\(.*?\),\s*imu_\s*\(.*?\),\s*encoder_\s*\(.*?\),\s*radio_\s*\(' `
  'XRobotModules construction order must match member dependency order.'
Assert-Contains $xrobotHeader 'GreySensor& GreySensorModule()' `
  'XRobotModules must expose its GreySensor module.'
Assert-Contains $xrobotHeader 'Tracking& TrackingModule()' `
  'XRobotModules must expose its Tracking module.'
Assert-Contains $xrobotHeader 'MPU6050& ImuModule()' `
  'XRobotModules must expose its MPU6050 module.'
Assert-Contains $xrobotHeader 'Module::Encoder& EncoderModule()' `
  'XRobotModules must expose its Encoder module.'
Assert-Contains $xrobotHeader 'NRF24L01& RadioModule()' `
  'XRobotModules must expose its NRF24L01 module.'
Assert-Contains $xrobotHeader 'HC_SR04& UltrasonicModule()' `
  'XRobotModules must expose its HC-SR04 module.'
Assert-Matches $xrobotHeader '(?s)NRF24L01\s+radio_;.*HC_SR04\s+ultrasonic_;' `
  'XRobotModules must own HC-SR04 after the existing module composition.'
Assert-Contains $xrobotHeader 'XRobotModules(const XRobotModules&) = delete;' `
  'XRobotModules must not be copy constructed.'
Assert-Contains $xrobotHeader 'XRobotModules(XRobotModules&&) = delete;' `
  'XRobotModules must not be move constructed.'
Assert-Contains $xrobotHeader 'operator=(const XRobotModules&) = delete;' `
  'XRobotModules must not be copy assigned.'
Assert-Contains $xrobotHeader 'operator=(XRobotModules&&) = delete;' `
  'XRobotModules must not be move assigned.'

Assert-Contains $appMain '#include "xrobot_main.hpp"' `
  'app_main must include the XRobotModules composition class.'
Assert-Contains $appMain '#include "mspm0_i2c.hpp"' `
  'app_main must include the MSPM0 I2C adapter.'
Assert-Contains $appMain 'MPU6050::BUFFER_SIZE' `
  'app_main must allocate the MPU6050 I2C stage buffer from BUFFER_SIZE.'
Assert-Matches $appMain 'MSPM0_I2C_INIT\s*\(\s*I2C_0\s*,' `
  'app_main must bind MPU6050 to the SysConfig I2C0 instance.'
Assert-Contains $appMain 'NRF24L01_CE_PIN' `
  'app_main must construct the NRF24L01 CE GPIO.'
Assert-Contains $appMain 'NRF24L01_CSN_PIN' `
  'app_main must construct the NRF24L01 CSN GPIO.'
Assert-Contains $appMain 'NRF24L01_SCK_PIN' `
  'app_main must construct the NRF24L01 SCK GPIO.'
Assert-Contains $appMain 'NRF24L01_MOSI_PIN' `
  'app_main must construct the NRF24L01 MOSI GPIO.'
Assert-Contains $appMain 'NRF24L01_MISO_PIN' `
  'app_main must construct the NRF24L01 MISO GPIO.'
Assert-Contains $appMain 'HC_SR04::Resources ultrasonic_resources' `
  'app_main must assemble the HC-SR04 board resources.'
Assert-Contains $appMain 'MSPM0_PWM_INIT(PWM_ULTRASONIC, GPIO_PWM_ULTRASONIC_C0)' `
  'app_main must bind the HC-SR04 trigger to the SysConfig PWM channel.'
Assert-Matches $appMain `
  '(?s)HC_SR04::Resources\s+ultrasonic_resources\s*\{.*CAPTURE_ULTRASONIC_INST\s*,\s*DL_TIMER_CC_0_INDEX\s*,\s*DL_TIMER_INTERRUPT_CC0_UP_EVENT\s*,\s*1000000U\s*\}' `
  'app_main must bind the HC-SR04 echo capture channel, event, and 1 MHz clock.'
Assert-Matches $appMain `
  '(?s)HardwareContainer\s+hardware\s*\(\s*LibXR::Entry<LibXR::I2C>.*LibXR::Entry<LibXR::GPIO>\s*\{\s*key_gpio_1.*LibXR::Entry<LibXR::GPIO>\s*\{\s*grey_sensor_gpio_0.*LibXR::Entry<LibXR::GPIO>\s*\{\s*nrf24l01_ce_gpio' `
  'HardwareContainer must register I2C first, then buttons/grey inputs, then NRF GPIOs.'

$managerDefinitions = [regex]::Matches(
  $appMain, 'LibXR::ApplicationManager\s+[A-Za-z_][A-Za-z0-9_]*\s*;')
if ($managerDefinitions.Count -ne 1) {
  throw 'app_main must construct exactly one ApplicationManager.'
}
Assert-Contains $appMain `
  'XRobotModules modules(hardware, app_manager, ultrasonic_resources, config);' `
  'app_main must construct the module composition once with HC-SR04 resources.'
Assert-Contains $appMain 'config.grey_active_low = kGreySensorActiveLow;' `
  'app_main must pass the board GreySensor polarity through XRobotModules::Config.'
Assert-Contains $appMain 'modules.TrackingModule()' `
  'app_main must obtain Tracking through XRobotModules.'
Assert-Contains $appMain 'modules.EncoderModule()' `
  'app_main must obtain Encoder through XRobotModules.'
Assert-Contains $carControlHeader 'struct CarControlSample' `
  'CarControlSupport must aggregate control-loop diagnostics in one sample.'
Assert-Matches $carControlHeader `
  'std::array<float,\s*Module::MotorGroup::kMotorCount>\s+motor_output' `
  'CarControlSample must expose coherent four-wheel motor outputs.'
Assert-Contains $appMain 'GreySensor::Sample grey_sensor_sample{};' `
  'app_main must expose the complete GreySensor sample for Ozone.'
Assert-Contains $appMain 'Tracking::Output tracking_output{};' `
  'app_main must expose the complete Tracking output for Ozone.'
Assert-Contains $appMain 'Module::Encoder::Sample encoder_sample{};' `
  'app_main must expose the complete Encoder sample for Ozone.'
Assert-Contains $appMain 'HC_SR04::Sample hc_sr04_sample{};' `
  'app_main must expose the complete HC-SR04 sample for Ozone.'
Assert-Contains $appMain 'MPU6050::Sample mpu6050_sample{};' `
  'app_main must expose the complete MPU6050 sample for Ozone.'
Assert-Contains $appMain 'NRF24L01::RxPacket nrf24l01_rx_packet{};' `
  'app_main must expose the complete NRF24L01 receive packet for Ozone.'
Assert-Contains $appMain 'NRF24L01::Status nrf24l01_status{};' `
  'app_main must expose the complete NRF24L01 status for Ozone.'
Assert-Contains $appMain 'BitsButtonXR::ButtonEventResult button_event{};' `
  'app_main must expose the complete button event for Ozone.'
Assert-Contains $appMain 'CarControlSample car_control_sample{};' `
  'app_main must expose one aggregate control sample for Ozone.'
Assert-Contains $appMain 'DriveMode drive_mode = DriveMode::kIdle;' `
  'The Ozone snapshot must not replace the private authoritative drive mode.'
Assert-Contains $appMain 'car_control_sample.drive_mode = drive_mode;' `
  'app_main must mirror the private drive mode into CarControlSample.'
Assert-NotMatches $appMain '(?m)^\s*GreySensor\s+[A-Za-z_][A-Za-z0-9_]*\s*\(' `
  'app_main must not directly construct GreySensor.'
Assert-NotMatches $appMain '(?m)^\s*Tracking\s+[A-Za-z_][A-Za-z0-9_]*\s*\(' `
  'app_main must not directly construct Tracking.'
Assert-NotMatches $appMain '(?m)^\s*MPU6050\s+[A-Za-z_][A-Za-z0-9_]*\s*\(' `
  'app_main must not directly construct MPU6050.'
Assert-NotMatches $appMain '(?m)^\s*Module::Encoder\s+[A-Za-z_][A-Za-z0-9_]*\s*[;(]' `
  'app_main must not directly construct Encoder.'
Assert-NotMatches $appMain '(?m)^\s*NRF24L01\s+[A-Za-z_][A-Za-z0-9_]*\s*\(' `
  'app_main must not directly construct NRF24L01.'

Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<Tracking::Output>' `
  'app_main must retain the Tracking topic subscriber.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<GreySensor::Sample>' `
  'app_main must receive complete GreySensor samples through its topic.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<Module::Encoder::Sample>' `
  'app_main must consume Encoder samples through an async topic subscriber.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<HC_SR04::Sample>' `
  'app_main must consume HC-SR04 samples through an async topic subscriber.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<MPU6050::Sample>' `
  'app_main must consume MPU6050 samples through an async topic subscriber.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<NRF24L01::RxPacket>' `
  'app_main must consume NRF24L01 receive packets through an async topic subscriber.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<NRF24L01::Status>' `
  'app_main must consume NRF24L01 status through an async topic subscriber.'
Assert-Matches $appMain `
  '(?s)if\s*\(hc_sr04_subscriber\.Available\(\)\).*hc_sr04_sample\s*=\s*hc_sr04_subscriber\.GetData\(\);.*hc_sr04_subscriber\.StartWaiting\(\);' `
  'app_main must update and rearm the HC-SR04 Ozone snapshot.'
Assert-Matches $appMain `
  '(?s)if\s*\(mpu6050_subscriber\.Available\(\)\).*mpu6050_sample\s*=\s*mpu6050_subscriber\.GetData\(\);.*mpu6050_subscriber\.StartWaiting\(\);' `
  'app_main must update and rearm the MPU6050 Ozone snapshot.'
Assert-Matches $appMain `
  '(?s)if\s*\(nrf24l01_rx_subscriber\.Available\(\)\).*nrf24l01_rx_packet\s*=\s*nrf24l01_rx_subscriber\.GetData\(\);.*nrf24l01_rx_subscriber\.StartWaiting\(\);' `
  'app_main must update and rearm the NRF24L01 receive Ozone snapshot.'
Assert-Matches $appMain `
  '(?s)if\s*\(nrf24l01_status_subscriber\.Available\(\)\).*nrf24l01_status\s*=\s*nrf24l01_status_subscriber\.GetData\(\);.*nrf24l01_status_subscriber\.StartWaiting\(\);' `
  'app_main must update and rearm the NRF24L01 status Ozone snapshot.'
Assert-Contains $appMain 'grey_sensor_subscriber.StartWaiting();' `
  'app_main must start waiting for GreySensor samples.'
Assert-Contains $appMain 'grey_sensor_subscriber.Available()' `
  'app_main must consume GreySensor samples after MonitorAll().'
Assert-Contains $appMain 'encoder_subscriber.StartWaiting();' `
  'app_main must start waiting for Encoder samples.'
Assert-Contains $appMain 'encoder_subscriber.Available()' `
  'app_main must consume Encoder samples after MonitorAll().'
Assert-Contains $carControl 'CalculateForwardDistanceM(const Module::Encoder::Sample&' `
  'Forward distance must be calculated from a coherent Encoder sample.'
Assert-Contains $appMain 'encoder_sample.speed_rad_s[i]' `
  'The speed loop must use the cached Encoder sample.'
Assert-Contains $appMain 'ResetEncoderState(' `
  'K1 reset must clear and restart the Encoder sample pipeline.'
Assert-Contains $carControlHeader 'constexpr uint32_t kTopicFreshnessTimeoutMs = 50U;' `
  'app_main must bound topic sample freshness to 50 ms.'
Assert-Contains $carControlHeader '#include "control_topic_logic.hpp"' `
  'car control support must expose the tested control topic logic to app_main.'
Assert-Matches $controlTopicLogic `
  'IsFresh\s*\([\s\S]*?now_ms\s*-\s*last_sample_time_ms[\s\S]*?<\s*timeout_ms' `
  'Topic freshness helper must use wrap-safe elapsed-time arithmetic.'
Assert-Matches $appMain 'ControlTopicLogic::IsFresh\s*\(' `
  'app_main must call the tested topic freshness helper.'
Assert-NotMatches $appMain '\bbool\s+IsTopicFresh\s*\(' `
  'app_main must not duplicate topic freshness logic locally.'
Assert-Contains $appMain 'last_tracking_sample_time_ms = now;' `
  'app_main must timestamp each received Tracking sample.'
Assert-Contains $appMain 'has_tracking_sample = true;' `
  'app_main must remember that a Tracking sample has arrived.'
Assert-Contains $appMain 'last_encoder_sample_time_ms = now;' `
  'app_main must timestamp each received Encoder sample.'
Assert-Contains $appMain 'has_encoder_sample = true;' `
  'app_main must remember that an Encoder sample has arrived.'
Assert-Matches $carControlSource `
  'ResetEncoderState\s*\([\s\S]*?ControlTopicLogic::InvalidateCache\s*\(\s*encoder_sample\s*,\s*has_encoder_sample\s*\)' `
  'K1 Encoder reset must invalidate the cached sample immediately.'
Assert-Matches $carControlSource `
  'ResetTrackingState\s*\([\s\S]*?ControlTopicLogic::InvalidateCache\s*\(\s*tracking_output\s*,\s*has_tracking_sample\s*\)' `
  'Tracking reset must invalidate the cached output immediately.'
Assert-Matches $carControlSource `
  'ResolveTargetSpeed\s*\([\s\S]*?ControlTopicLogic::GateTarget\s*\([\s\S]*?drive_mode\s*==\s*DriveMode::kLineFollowing' `
  'app_main target generation must call the tested Encoder/Tracking gate.'
Assert-Matches $appMain `
  'car_control_sample\.line_lost\s*=\s*!tracking_topic_fresh\s*\|\|\s*tracking_output\.lost_line\s*;' `
  'A stale Tracking topic must be reported as line loss.'
Assert-Matches $appMain `
  'float\s+measured\s*=\s*encoder_topic_fresh\s*\?[\s\S]*?:\s*0\.0[fF]\s*;' `
  'Stale Encoder speed must not enter the speed controller.'
Assert-Matches $appMain `
  'ControlTopicLogic::ShouldRunSpeedController\s*\(\s*drive_mode\s*!=\s*DriveMode::kIdle\s*,\s*encoder_topic_fresh\s*\)' `
  'The PID controller must call the tested active-mode/Encoder gate.'
Assert-NotContains $appMain 'encoder.Init()' `
  'app_main must not use the legacy Encoder::Init API.'
Assert-NotContains $appMain 'encoder.GetSpeed(' `
  'app_main must not use the legacy Encoder::GetSpeed API.'
Assert-NotContains $appMain 'encoder.GetAngle(' `
  'app_main must not use the legacy Encoder::GetAngle API.'
Assert-NotContains $appMain 'encoder.GetCount(' `
  'app_main must not use the legacy Encoder::GetCount API.'

$legacyDebugContent = $carControlHeader + $carControlSource + $appMain
Assert-NotMatches $legacyDebugContent `
  '\bg_(elapsed_ms|dt_s|encoder_count|encoder_angle|encoder_speed|line_raw_mask|line_black_mask|line_active_count|line_error|line_lost|line_following_enabled|grey_sensor_active_low|tracking_topic_ready|tracking_sequence|tracking_source_sequence|tracking_left_speed|tracking_right_speed|drive_mode|last_button_index|last_button_event|forward_distance_m|target_speed|measured_speed|feedforward_output|pid_output|motor_output)\b' `
  'Field-by-field volatile debug mirrors must be replaced by complete samples.'
Assert-NotMatches $legacyDebugContent '\bjie\b' `
  'The unused jie debug symbol must be removed.'
Assert-NotContains $appMain 'NRF24L01_Init' `
  'app_main must not use the legacy NRF24L01 initialization API.'
Assert-NotContains $appMain 'NRF24L01_Receive' `
  'app_main must not use the legacy NRF24L01 receive API.'

Write-Host 'XRobot module management structure checks passed.'
