$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$checkScript = Join-Path $root 'cmake/CheckFirmwareSize.cmake'
$rootCMake = Join-Path $root 'CMakeLists.txt'
$limitBytes = 128450

if (-not (Test-Path -LiteralPath $checkScript -PathType Leaf)) {
  throw "Missing firmware size check script: $checkScript"
}

$cmakeContent = Get-Content -Raw -Encoding UTF8 -LiteralPath $rootCMake
if ($cmakeContent -notmatch '(?s)-O\s+binary.*CheckFirmwareSize\.cmake') {
  throw 'Firmware size check must run after the binary objcopy command.'
}
if (-not $cmakeContent.Contains('FIRMWARE_MAX_SIZE_BYTES=128450')) {
  throw 'Firmware POST_BUILD size limit must be 128450 bytes.'
}

$testDirectory = Join-Path $root "build/flash-budget-test-$PID"
$smallFile = Join-Path $testDirectory 'small.bin'
$largeFile = Join-Path $testDirectory 'large.bin'

try {
  New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
  [System.IO.File]::WriteAllBytes($smallFile, [byte[]]::new($limitBytes))
  [System.IO.File]::WriteAllBytes($largeFile, [byte[]]::new($limitBytes + 1))

  & cmake "-DFIRMWARE_BIN=$smallFile" `
    "-DFIRMWARE_MAX_SIZE_BYTES=$limitBytes" -P $checkScript 2>&1 | Out-Null
  if ($LASTEXITCODE -ne 0) {
    throw 'Firmware size check rejected a binary at the configured limit.'
  }

  $savedErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  & cmake "-DFIRMWARE_BIN=$largeFile" `
    "-DFIRMWARE_MAX_SIZE_BYTES=$limitBytes" -P $checkScript 2>&1 | Out-Null
  $largeFileExitCode = $LASTEXITCODE
  $ErrorActionPreference = $savedErrorActionPreference
  if ($largeFileExitCode -eq 0) {
    throw 'Firmware size check accepted a binary above the configured limit.'
  }
} finally {
  if (Test-Path -LiteralPath $testDirectory) {
    Remove-Item -Recurse -Force -LiteralPath $testDirectory
  }
}

Write-Host 'Firmware flash budget checks passed.'
