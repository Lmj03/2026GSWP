param(
    [ValidateRange(1024, 65535)]
    [int]$Port = 8765,
    [switch]$OpenBrowser,
    [switch]$NoRecord
)

$ErrorActionPreference = 'Stop'
$pythonPath = $null
$pythonCommand = Get-Command python -ErrorAction SilentlyContinue
if ($pythonCommand -and $pythonCommand.Source -notlike '*\WindowsApps\*')
{
    $pythonPath = $pythonCommand.Source
}
if (-not $pythonPath)
{
    $bundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundledPython)
    {
        $pythonPath = $bundledPython
    }
}
if (-not $pythonPath)
{
    throw 'Python 3.9+를 설치한 뒤 실행하세요. 또는 python tools/monitor_server.py 명령으로 시작하세요.'
}
$monitorArguments = @((Join-Path $PSScriptRoot 'monitor_server.py'), '--port', "$Port")
if ($OpenBrowser)
{
    $monitorArguments += '--open-browser'
}
if ($NoRecord)
{
    $monitorArguments += '--no-record'
}
& $pythonPath @monitorArguments
exit $LASTEXITCODE
