param(
    [string]$BuildDirectory = ""
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot "build"
}
$compileDatabase = Join-Path $BuildDirectory "compile_commands.json"
if (-not (Test-Path $compileDatabase)) {
    throw "Missing compile_commands.json: configure the firmware build first"
}

$entries = Get-Content -Raw -Encoding UTF8 $compileDatabase | ConvertFrom-Json
$entry = $entries | Where-Object { $_.file -like "*app_main.cpp" } | Select-Object -First 1
if ($null -eq $entry) {
    throw "No app_main.cpp entry found in compile_commands.json"
}
$tokens = @($entry.command -split ' ')
$outputIndex = [Array]::IndexOf($tokens, '-o')
if ($outputIndex -lt 2) {
    throw "Unsupported compile command format"
}
$compiler = $tokens[0]
$compileArgs = @($tokens[1..($outputIndex - 1)])
$outputDirectory = Join-Path $repositoryRoot ".cache/module-header-odr"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$sources = @(
    (Join-Path $PSScriptRoot "module_header_odr_a.cpp"),
    (Join-Path $PSScriptRoot "module_header_odr_b.cpp"),
    (Join-Path $PSScriptRoot "module_constructor_contract.cpp")
)
$objects = @()

Push-Location $entry.directory
try {
    foreach ($source in $sources) {
        $object = Join-Path $outputDirectory ((Split-Path -Leaf $source) + ".o")
        & $compiler @compileArgs -c $source -o $object
        if ($LASTEXITCODE -ne 0) {
            throw "Header translation unit compile failed: $source"
        }
        $objects += $object
    }
    $linked = Join-Path $outputDirectory "module_headers.o"
    & $compiler -r -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft @objects -o $linked
    if ($LASTEXITCODE -ne 0) {
        throw "Header ODR link failed"
    }
} finally {
    Pop-Location
}

Write-Host "Module header compile and ODR verification passed."
