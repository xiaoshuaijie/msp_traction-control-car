$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$fakeInclude = Join-Path $PSScriptRoot 'fakes'
$moduleInclude = Join-Path $root 'module/NRF24L01'
$adapterSource = Join-Path $moduleInclude 'NRF24L01App.cpp'
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
  '-D__NRF24L01_DEFINE_H' `
  '-DNRF24L01_ADDRESS_WIDTH=5U' `
  '-DNRF24L01_TX_PACKET_WIDTH=32U' `
  '-DNRF24L01_RX_PACKET_WIDTH=32U' `
  '-I' $fakeInclude `
  '-I' $moduleInclude `
  $adapterSource `
  $testSource `
  '-o' $executable

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

& $executable
exit $LASTEXITCODE
