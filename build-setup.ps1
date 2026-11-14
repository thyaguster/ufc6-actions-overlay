param([switch]$Test)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The Windows .NET Framework C# compiler is required to build setup.' }
New-Item -ItemType Directory -Force -Path 'dist','.build','release' | Out-Null
$release = Get-Content -LiteralPath 'dist\release.json' -Raw | ConvertFrom-Json
$arguments = @('/nologo','/codepage:65001','/target:winexe','/platform:x64','/optimize+','/warn:4','/warnaserror+','/out:dist\UFC6ActionsOverlaySetup.exe','/win32manifest:installer\setup.manifest','/win32icon:installer\assets\overlay.ico','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll','/resource:installer\assets\overlay.png,overlay-brand.png','/resource:dist\diagnostics-components.json,diagnostics-components.json')
foreach ($name in @('libwinpthread-1.dll','winmm.dll','version.dll','ufc6_menu_layer.dll','ufc6_menu_layer.json','ufc6_menu_restart.exe','ufc6_menu_icons.exe','ufc6_menu_update.exe','release.json')) {
    $arguments += "/resource:dist\$name,$name"
}
$arguments += @('installer\Setup.cs','installer\SelfTests.cs','installer\AssemblyInfo.cs','installer\UpdateClient.cs','installer\UpdateForm.cs','installer\ModernUi.cs','installer\FolderPicker.cs','installer\Diagnostics.cs','installer\DiagnosticsForm.cs','installer\DiagnosticsTests.cs')
& $compiler @arguments
if ($LASTEXITCODE) { throw 'Setup compilation failed.' }
if ($Test) {
    $report = Join-Path $PSScriptRoot '.build\setup-tests.txt'
    $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'dist\UFC6ActionsOverlaySetup.exe') -ArgumentList @('--self-test',('"' + $report + '"')) -WindowStyle Hidden -PassThru -Wait
    Get-Content -LiteralPath $report
    if ($process.ExitCode) { throw 'Setup integration tests failed.' }
    $updateReport=Join-Path $PSScriptRoot '.build\update-tests.txt'
    $updateProcess=Start-Process -FilePath (Join-Path $PSScriptRoot 'dist\ufc6_menu_update.exe') -ArgumentList @('--self-test',('"'+$updateReport+'"')) -WindowStyle Hidden -PassThru -Wait
    Get-Content -LiteralPath $updateReport
    if($updateProcess.ExitCode){throw 'Update helper tests failed.'}

}
$output = Join-Path $PSScriptRoot "release\UFC6-Actions-Overlay-Setup-$($release.version).exe"
Copy-Item -LiteralPath 'dist\UFC6ActionsOverlaySetup.exe' -Destination $output -Force
Get-Item -LiteralPath $output | Select-Object FullName,Length | Format-List
