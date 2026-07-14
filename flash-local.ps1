$ErrorActionPreference = "Stop"
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

$idfPath = "C:\Users\Ola Andersson\termoflow-test\esp-idf"
. "$idfPath\export.ps1"

Set-Location "C:\Users\Ola Andersson\termoflow-test\ThermoFlow-repo"
$port = if ($args.Count -gt 0) { $args[0] } else { "COM4" }

# app-flash updates only the application partition and preserves NVS (WiFi, device name, etc.)
idf.py -p $port app-flash
exit $LASTEXITCODE