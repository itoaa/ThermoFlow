$ErrorActionPreference = "Stop"
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

$idfPath = "C:\Users\Ola Andersson\termoflow-test\esp-idf"
. "$idfPath\export.ps1"

Set-Location "C:\Users\Ola Andersson\termoflow-test\ThermoFlow-repo"
$env:SDKCONFIG_DEFAULTS = "sdkconfig.ci.defaults"
$env:CHANNEL = "dev"
$env:BUILD_NUMBER = "0"
$env:REVISION = "1"
$env:USE_BUILD_VERSION = "1"

try {
    $env:GIT_SHA = (git rev-parse --short=7 HEAD 2>$null)
} catch {
    $env:GIT_SHA = "unknown"
}

$pythonExe = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
if (-not (Test-Path $pythonExe)) {
    $python = Get-Command python3 -ErrorAction SilentlyContinue
    if (-not $python) {
        $python = Get-Command python -ErrorAction SilentlyContinue
    }
    if ($python) {
        $pythonExe = $python.Source
    }
}
if (Test-Path $pythonExe) {
    & $pythonExe scripts/generate_version.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "Python not found; using committed include/thermoflow_version.h"
}

if (-not (Test-Path "sdkconfig")) {
    idf.py set-target esp32s3
}

idf.py build
exit $LASTEXITCODE