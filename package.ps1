$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$version = (Get-Content -LiteralPath 'dist\release.json' -Raw | ConvertFrom-Json).version
& (Join-Path $PSScriptRoot 'build-setup.ps1')
$package = Join-Path $PSScriptRoot "release\UFC6-Actions-Overlay-$version"
New-Item -ItemType Directory -Force -Path $package, "$package\dist" | Out-Null
foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','RELEASE-NOTES.md','build.ps1','build-setup.ps1','manage_menu.py','attach_menu.py')) {
    Copy-Item -LiteralPath $file -Destination $package
}
Copy-Item -LiteralPath 'src' -Destination $package -Recurse -Force
Copy-Item -LiteralPath 'installer' -Destination $package -Recurse -Force
if (Test-Path -LiteralPath 'docs') { Copy-Item -LiteralPath 'docs' -Destination $package -Recurse -Force }
if (Test-Path -LiteralPath '.github') { Copy-Item -LiteralPath '.github' -Destination $package -Recurse -Force }
Copy-Item -LiteralPath 'dist\UFC6ActionsOverlaySetup.exe' -Destination (Join-Path $package 'UFC6ActionsOverlaySetup.exe')
New-Item -ItemType Directory -Force -Path "$package\tests" | Out-Null
Get-ChildItem -LiteralPath 'tests' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination "$package\tests"
}
Copy-Item -LiteralPath 'dist\version.dll','dist\release.json','dist\ufc6_menu_layer.dll','dist\ufc6_menu_layer.json','dist\ufc6_menu_restart.exe' -Destination "$package\dist"
New-Item -ItemType Directory -Force -Path "$package\vendor\vulkan-headers" | Out-Null
Copy-Item -LiteralPath 'vendor\vulkan-headers\include','vendor\vulkan-headers\LICENSES' -Destination "$package\vendor\vulkan-headers" -Recurse -Force
Copy-Item -LiteralPath 'vendor\vulkan-headers\LICENSE.md' -Destination "$package\vendor\vulkan-headers"
$archive = Join-Path $PSScriptRoot "release\UFC6-Actions-Overlay-$version.zip"
Compress-Archive -LiteralPath $package -DestinationPath $archive -Force
Get-Item -LiteralPath $archive | Select-Object FullName,Length | Format-List
