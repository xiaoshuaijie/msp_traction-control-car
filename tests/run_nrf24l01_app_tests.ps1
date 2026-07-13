$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$moduleInclude = Join-Path $root 'module/NRF24L01'
$testSource = Join-Path $PSScriptRoot 'nrf24l01_app_test.cpp'
$buildDirectory = Join-Path $root 'build'
$executable = Join-Path $buildDirectory 'nrf24l01_app_test.exe'

New-Item -ItemType Directory -Force $buildDirectory | Out-Null

& g++ `
  '-std=c++2a' `
  '-Wall' `
  '-Wextra' `
  '-Werror' `
  '-pedantic' `
  '-I' $moduleInclude `
  $testSource `
  '-o' $executable

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

& $executable
exit $LASTEXITCODE
