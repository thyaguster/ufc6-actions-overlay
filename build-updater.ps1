$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
New-Item -ItemType Directory -Force -Path dist | Out-Null
$checks = [ordered]@{
    'UFC 6.exe'='f3c42126bd9df5c12cb69c1c57233a94ba024c50929ba0dfe20e715521f691a2'
    'UFC 6 Parsec.exe'='ad94dc7e30ad12c8078d14d96520c1d48d4b0c658604cc3dcc1d68a3e228773d'
    'UFC 6 novo.exe'='ace7a59392b47a776d469768f4ed1aef2c3395c26ac4ed57fd67661c2d6abe48'
    'ufc6_pthread_original.dll'='78ebbc1480d62098aa66307db2ff139abc07c137bc188f5b73bf7b131492932c'
}
foreach ($name in @('version.dll','libwinpthread-1.dll','winmm.dll','ufc6_menu_layer.dll','ufc6_menu_layer.json','ufc6_menu_restart.exe','ufc6_menu_icons.exe')) {
    $checks[$name]=(Get-FileHash -LiteralPath (Join-Path dist $name)).Hash.ToLowerInvariant()
}
$checks | ConvertTo-Json | Set-Content -LiteralPath 'dist/diagnostics-components.json' -Encoding utf8
& $compiler /nologo /codepage:65001 /target:winexe /platform:x64 /optimize+ /warn:4 /warnaserror+ /define:OVERLAY_UPDATER '/out:dist\ufc6_menu_update.exe' '/win32icon:installer\assets\overlay.ico' '/win32manifest:installer\setup.manifest' /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll '/resource:installer\assets\overlay.png,overlay-brand.png' '/resource:dist\diagnostics-components.json,diagnostics-components.json' 'installer\ModernUi.cs' 'installer\UpdateClient.cs' 'installer\UpdateForm.cs' 'installer\UpdateProgram.cs' 'installer\UpdateTests.cs' 'installer\AssemblyInfo.cs' 'installer\Diagnostics.cs' 'installer\DiagnosticsForm.cs' 'installer\DiagnosticsTests.cs'
if($LASTEXITCODE){throw 'Update helper compilation failed.'}
