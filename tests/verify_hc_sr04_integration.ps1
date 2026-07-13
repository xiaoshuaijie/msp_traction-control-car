$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

function Assert-Contains {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file not found: $Path"
    }

    $content = Get-Content -Raw -Encoding UTF8 -LiteralPath $Path
    if (-not [regex]::IsMatch($content, $Pattern)) {
        throw "Required pattern not found in ${Path}: $Pattern"
    }
}

function Assert-InitializerContains {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$InitializerPattern,
        [Parameter(Mandatory = $true)]
        [string]$FieldPattern
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file not found: $Path"
    }

    $content = Get-Content -Raw -Encoding UTF8 -LiteralPath $Path
    $blockPattern = "(?s)(?:$InitializerPattern)\s*=\s*\{(?<body>(?:(?!\};).)*)\};"
    $blockMatch = [regex]::Match($content, $blockPattern)
    if (-not $blockMatch.Success) {
        throw "Required initializer not found in ${Path}: $InitializerPattern"
    }

    if (-not [regex]::IsMatch($blockMatch.Groups['body'].Value, $FieldPattern)) {
        throw "Required field not found in initializer ${InitializerPattern}: $FieldPattern"
    }
}

$syscfgPath = Join-Path $root 'sysconfig/untitled.syscfg'
$generatedConfigPath = Join-Path $root 'sysconfig/ti_msp_dl_config.c'
$generatedHeaderPath = Join-Path $root 'sysconfig/ti_msp_dl_config.h'
$topLevelCMakePath = Join-Path $root 'CMakeLists.txt'
$moduleCMakePath = Join-Path $root 'module/HC_SR04/CMakeLists.txt'
$pwmHeaderPath = Join-Path $root 'libxr/driver/mspm0/mspm0_pwm.hpp'

Assert-Contains $syscfgPath 'CAPTURE1\.timerPeriod\s*=\s*"50ms"\s*;'
Assert-Contains $syscfgPath 'CAPTURE1\.timerClkPrescale\s*=\s*40\s*;'
Assert-Contains $syscfgPath 'CAPTURE1\.captMode\s*=\s*"PULSE_WIDTH"\s*;'
Assert-Contains $syscfgPath 'PWM3\.timerStartTimer\s*=\s*false\s*;'
Assert-InitializerContains $generatedConfigPath 'static\s+const\s+DL_TimerG_ClockConfig\s+gCAPTURE_ULTRASONICClockConfig' '\.prescale\s*=\s*39U\b'
Assert-InitializerContains $generatedConfigPath 'static\s+const\s+DL_TimerG_CaptureConfig\s+gCAPTURE_ULTRASONICCaptureConfig' '\.captureMode\s*=\s*DL_TIMER_CAPTURE_MODE_PULSE_WIDTH_UP\s*,'
Assert-InitializerContains $generatedConfigPath 'static\s+const\s+DL_TimerG_PWMConfig\s+gPWM_ULTRASONICConfig' '\.startTimer\s*=\s*DL_TIMER_STOP\s*,'
Assert-Contains $generatedHeaderPath '(?m)^[ \t]*#define[ \t]+CAPTURE_ULTRASONIC_INST_LOAD_VALUE[ \t]+\(49999U\)[ \t]*\r?$'
Assert-Contains $topLevelCMakePath 'add_subdirectory\(\s*"\$\{CMAKE_SOURCE_DIR\}/module/HC_SR04"\s*\)'
Assert-Contains $moduleCMakePath 'target_include_directories\(\s*xr\s+PUBLIC\s+\$\{CMAKE_CURRENT_LIST_DIR\}\s*\)'
Assert-Contains $moduleCMakePath '(?s)target_sources\(\s*xr\s+PRIVATE(?:(?!\)).)*"\$\{CMAKE_CURRENT_LIST_DIR\}/sr04\.cpp"\s*\)'
Assert-Contains $pwmHeaderPath '(?s)#define\s+MSPM0_PWM_INIT\(timer_name,\s*gpio_name\).*DL_Timer_getClockConfig.*divideRatio.*prescale.*MSPM0PWM::Resources'

Write-Output 'HC-SR04 integration checks passed.'
