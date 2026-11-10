$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
New-Item -ItemType Directory -Force -Path dist | Out-Null
& $compiler /nologo /codepage:65001 /target:winexe /platform:x64 /optimize+ /warn:4 /warnaserror+ /define:OVERLAY_UPDATER '/out:dist\ufc6_menu_update.exe' '/win32icon:installer\assets\overlay.ico' '/win32manifest:installer\setup.manifest' /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll 'installer\UpdateClient.cs' 'installer\UpdateForm.cs' 'installer\UpdateProgram.cs' 'installer\UpdateTests.cs' 'installer\AssemblyInfo.cs'
if($LASTEXITCODE){throw 'Update helper compilation failed.'}
