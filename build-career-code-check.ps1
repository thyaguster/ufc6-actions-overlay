param([switch]$Test)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
New-Item -ItemType Directory -Path release,.build -Force | Out-Null
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /warn:4 /warnaserror+ '/out:release\UFC6-Career-Code-Check.exe' '/win32manifest:installer\setup.manifest' '/win32icon:installer\assets\overlay.ico' /reference:System.Windows.Forms.dll installer\CareerCodeCheck.cs
if($LASTEXITCODE){throw 'Career code checker compilation failed.'}
if($Test){
 $report=Join-Path $PSScriptRoot '.build\career-code-check-selftest.txt'
 $checker=Start-Process -FilePath (Join-Path $PSScriptRoot 'release\UFC6-Career-Code-Check.exe') -ArgumentList @('--self-test',('"'+$report+'"')) -WindowStyle Hidden -PassThru -Wait
 if($checker.ExitCode){throw 'Career code checker self-test failed.'}
 Get-Content -LiteralPath $report
}
$artifact=Get-Item -LiteralPath 'release\UFC6-Career-Code-Check.exe'
$digest=(Get-FileHash -LiteralPath $artifact.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath ($artifact.FullName+'.sha256') -Value ($digest+'  '+$artifact.Name) -Encoding ascii
$artifact | Select-Object FullName,Length
