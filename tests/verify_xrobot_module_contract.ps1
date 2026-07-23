param(
    [string]$Generator = "xrobot_gen_main"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$yamlPath = Join-Path $repositoryRoot "User/xrobot.yaml"
$checkedGeneratedPath = Join-Path $repositoryRoot "User/xrobot_main.hpp"
$generatedPath = Join-Path $repositoryRoot ".cache/xrobot-contract/xrobot_main.hpp"
$moduleHeaders = @(
    (Join-Path $repositoryRoot "Modules/MPU6050/MPU6050.hpp"),
    (Join-Path $repositoryRoot "Modules/NRF24L01/NRF24L01.hpp")
)

$generatedDirectory = Split-Path -Parent $generatedPath
New-Item -ItemType Directory -Force -Path $generatedDirectory | Out-Null
& $Generator -c $yamlPath -o $generatedPath
if ($LASTEXITCODE -ne 0) {
    throw "xrobot_gen_main failed with exit code $LASTEXITCODE"
}

$yaml = Get-Content -Raw -Encoding UTF8 $yamlPath
$generated = Get-Content -Raw -Encoding UTF8 $generatedPath
$checkedGenerated = Get-Content -Raw -Encoding UTF8 $checkedGeneratedPath
if (($generated -replace "`r`n", "`n") -ne
    ($checkedGenerated -replace "`r`n", "`n")) {
    throw "User/xrobot_main.hpp is stale; regenerate it from User/xrobot.yaml"
}
$targets = @($yaml, $generated, $checkedGenerated)
foreach ($header in $moduleHeaders) {
    $targets += Get-Content -Raw -Encoding UTF8 $header
    $lineCount = (Get-Content -Encoding UTF8 $header).Count
    if ($lineCount -gt 500) {
        throw "Module header exceeds 500 lines: $header ($lineCount)"
    }
    $sourceFiles = Get-ChildItem -Path (Split-Path -Parent $header) -File |
        Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".cxx") }
    if ($sourceFiles) {
        throw "Module contains implementation source files: $($sourceFiles.FullName -join ', ')"
    }
}
foreach ($target in $targets) {
    if ($target -match "(?:MPU6050|NRF24L01)::Config\s*\{") {
        throw "Legacy Config{} construction remains in a target contract file"
    }
}

function Assert-InOrder {
    param([string]$Text, [string[]]$Tokens, [string]$Label)
    $position = 0
    foreach ($token in $Tokens) {
        $next = $Text.IndexOf($token, $position, [StringComparison]::Ordinal)
        if ($next -lt 0) {
            throw "$Label is missing or reorders token: $token"
        }
        $position = $next + $token.Length
    }
}

$mpuTokens = @(
    'static MPU6050 MPU6050_0(', '      "mpu6050",', '      10,', '      100,',
    '      200,', '      MPU6050::Filter::BAND_5HZ,',
    '      MPU6050::GyroRange::DPS_250,', '      MPU6050::AccelRange::G_2,',
    '      200,', '      1.2,', '      20.0,', '      0.05,', '      30.0,',
    '      0.1,', '      0.002,', '      20,', '      -2.3,', '      5.0,',
    '      0.0,', '      3e-05,', '      2048,',
    '      LibXR::Thread::Priority::HIGH'
)
$nrfTokens = @(
    'static NRF24L01 NRF24L01_0(', '      {17, 34, 51, 68, 85},',
    '      {17, 34, 51, 68, 85},', '      2,',
    '      NRF24L01::DataRate::MBPS_2,',
    '      NRF24L01::OutputPower::ZERO_DBM,', '      250,', '      3,',
    '      NRF24L01::PayloadMode::FIXED_32,', '      "nrf24l01_tx",',
    '      "nrf24l01_rx",', '      "nrf24l01_status",', '      10,',
    '      10,', '      100,', '      1000,', '      1000,', '      1,',
    '      1536,', '      LibXR::Thread::Priority::MEDIUM'
)
Assert-InOrder -Text $generated -Tokens $mpuTokens -Label "MPU6050 constructor"
Assert-InOrder -Text $generated -Tokens $nrfTokens -Label "NRF24L01 constructor"

Write-Host "XRobot module contract verification passed."
