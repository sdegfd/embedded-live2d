$enginePath = Split-Path -Parent $MyInvocation.MyCommand.Path

[Environment]::SetEnvironmentVariable('PainterEnginePath', $enginePath, 'User')
$env:PainterEnginePath = $enginePath

Write-Host "PainterEnginePath=$enginePath"
Write-Host 'User environment variable updated.'