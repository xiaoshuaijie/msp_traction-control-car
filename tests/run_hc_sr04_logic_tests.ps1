$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $PSScriptRoot 'hc_sr04_logic_test.cpp'
$buildDirectory = Join-Path $root 'build'
$executable = Join-Path $buildDirectory 'hc_sr04_logic_test.exe'

New-Item -ItemType Directory -Force $buildDirectory | Out-Null

& g++ -std=c++17 -Wall -Wextra -Werror -pedantic `
  -I $root `
  $source `
  -o $executable

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

& $executable
exit $LASTEXITCODE
