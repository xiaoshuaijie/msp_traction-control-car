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

$syscfgPath = Join-Path $root 'sysconfig/untitled.syscfg'
$generatedConfigPath = Join-Path $root 'sysconfig/ti_msp_dl_config.c'
$topLevelCMakePath = Join-Path $root 'CMakeLists.txt'
$moduleCMakePath = Join-Path $root 'module/HC_SR04/CMakeLists.txt'

Assert-Contains $syscfgPath 'CAPTURE1\.captMode\s*=\s*"PULSE_WIDTH"\s*;'
Assert-Contains $syscfgPath 'PWM3\.timerStartTimer\s*=\s*false\s*;'
Assert-Contains $generatedConfigPath '(?s)gCAPTURE_ULTRASONICCaptureConfig\s*=\s*\{.*?\.captureMode\s*=\s*DL_TIMER_CAPTURE_MODE_PULSE_WIDTH_UP\s*,.*?\};'
Assert-Contains $generatedConfigPath '(?s)gPWM_ULTRASONICConfig\s*=\s*\{.*?\.startTimer\s*=\s*DL_TIMER_STOP\s*,.*?\};'
Assert-Contains $topLevelCMakePath 'add_subdirectory\(\s*"\$\{CMAKE_SOURCE_DIR\}/module/HC_SR04"\s*\)'
Assert-Contains $moduleCMakePath 'target_sources\(\s*xr\s+PRIVATE\b'

Write-Output 'HC-SR04 integration checks passed.'
