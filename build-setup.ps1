param([switch]$Test)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The Windows .NET Framework C# compiler is required to build setup.' }
New-Item -ItemType Directory -Force -Path 'dist','.build','release' | Out-Null
$release = Get-Content -LiteralPath 'dist\release.json' -Raw | ConvertFrom-Json
$arguments = @('/nologo','/target:winexe','/platform:x64','/optimize+','/warn:4','/warnaserror+','/out:dist\UFC6ActionsOverlaySetup.exe','/win32manifest:installer\setup.manifest','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll')
foreach ($name in @('version.dll','ufc6_menu_layer.dll','ufc6_menu_layer.json','ufc6_menu_restart.exe','release.json')) {
    $arguments += "/resource:dist\$name,$name"
}
$arguments += @('installer\Setup.cs','installer\SelfTests.cs','installer\AssemblyInfo.cs')
& $compiler @arguments
if ($LASTEXITCODE) { throw 'Setup compilation failed.' }
if ($Test) {
    $report = Join-Path $PSScriptRoot '.build\setup-tests.txt'
    $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'dist\UFC6ActionsOverlaySetup.exe') -ArgumentList @('--self-test',('"' + $report + '"')) -WindowStyle Hidden -PassThru -Wait
    Get-Content -LiteralPath $report
    if ($process.ExitCode) { throw 'Setup integration tests failed.' }
}
$output = Join-Path $PSScriptRoot "release\UFC6-Actions-Overlay-Setup-$($release.version).exe"
Copy-Item -LiteralPath 'dist\UFC6ActionsOverlaySetup.exe' -Destination $output -Force
Get-Item -LiteralPath $output | Select-Object FullName,Length | Format-List
