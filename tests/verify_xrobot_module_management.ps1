$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

function Read-RequiredFile {
  param([string]$Path, [string]$MissingMessage)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw $MissingMessage
  }
  return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path
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

$adapterHeaderPath = Join-Path $root 'module/NRF24L01/NRF24L01App.hpp'
$adapterSourcePath = Join-Path $root 'module/NRF24L01/NRF24L01App.cpp'
$nrfCMakePath = Join-Path $root 'module/NRF24L01/CMakeLists.txt'

$adapterHeader = Read-RequiredFile $adapterHeaderPath `
  'Missing NRF24L01 LibXR adapter header.'
$adapterSource = Read-RequiredFile $adapterSourcePath `
  'Missing NRF24L01 LibXR adapter implementation.'
$nrfCMake = Read-RequiredFile $nrfCMakePath `
  'Missing NRF24L01 CMake configuration.'

Assert-Contains $adapterHeader 'class NRF24L01App : public LibXR::Application' `
  'NRF24L01App must be managed by ApplicationManager.'
Assert-Contains $adapterHeader 'const State& GetState() const' `
  'NRF24L01App must expose read-only runtime state.'
Assert-Contains $adapterSource 'NRF24L01_Init();' `
  'NRF24L01App must initialize the existing driver.'
Assert-Contains $adapterSource 'NRF24L01_Receive();' `
  'NRF24L01App must poll the existing driver.'
Assert-Contains $nrfCMake '"${CMAKE_CURRENT_LIST_DIR}/*.cpp"' `
  'NRF24L01 CMake must compile C++ sources.'

Write-Host 'XRobot module management structure checks passed.'
