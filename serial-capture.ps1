$ErrorActionPreference = "Stop"
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

$idfPath = "C:\Users\Ola Andersson\termoflow-test\esp-idf"
$repoPath = "C:\Users\Ola Andersson\termoflow-test\ThermoFlow-repo"
. "$idfPath\export.ps1"
Set-Location $repoPath

$port = if ($args.Count -gt 0) { $args[0] } else { "COM4" }
$seconds = if ($args.Count -gt 1) { [int]$args[1] } else { 25 }

$python = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
$idfScript = Join-Path $idfPath "tools\idf.py"
if (-not (Test-Path $python)) {
    throw "IDF Python not found at $python (run export.ps1 first)"
}

$logOut = Join-Path $env:TEMP ("thermoflow-serial-{0}.out.log" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
$logErr = "${logOut}.err"

Write-Host "Capturing $seconds s from $port"

$proc = Start-Process `
    -FilePath $python `
    -ArgumentList @(
        "`"$idfScript`"",
        "-p", $port,
        "monitor"
    ) `
    -WorkingDirectory $repoPath `
    -RedirectStandardOutput $logOut `
    -RedirectStandardError $logErr `
    -PassThru

Start-Sleep -Seconds $seconds

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Get-CimInstance Win32_Process -Filter "ParentProcessId=$($proc.Id)" -ErrorAction SilentlyContinue |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

if (Test-Path $logOut) { Get-Content $logOut }
if (Test-Path $logErr) { Get-Content $logErr }

if (-not (Test-Path $logOut) -and -not (Test-Path $logErr)) {
    Write-Host "No serial output captured."
}

exit 0