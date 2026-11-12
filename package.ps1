$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$version = (Get-Content -LiteralPath 'dist\release.json' -Raw | ConvertFrom-Json).version
& (Join-Path $PSScriptRoot 'build-setup.ps1')
$package = Join-Path $PSScriptRoot "release\UFC6-Actions-Overlay-$version"
New-Item -ItemType Directory -Force -Path $package, "$package\dist" | Out-Null
foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','RELEASE-NOTES.md','build.ps1','build-setup.ps1','build-icons.ps1','build-updater.ps1','package.ps1','manage_menu.py','attach_menu.py')) {
    Copy-Item -LiteralPath $file -Destination $package
}
Copy-Item -LiteralPath 'src' -Destination $package -Recurse -Force
Copy-Item -LiteralPath 'installer' -Destination $package -Recurse -Force
New-Item -ItemType Directory -Force -Path "$package\docs", "$package\docs\screenshots" | Out-Null
foreach ($name in @('PHOTO-MODE.md','ROADMAP.md','theme-config.example.cfg')) { Copy-Item -LiteralPath (Join-Path 'docs' $name) -Destination "$package\docs" }
Get-ChildItem -LiteralPath 'docs/screenshots' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination "$package\docs\screenshots" }
if (Test-Path -LiteralPath 'scripts') { Copy-Item -LiteralPath 'scripts' -Destination $package -Recurse -Force }
if (Test-Path -LiteralPath '.github') { Copy-Item -LiteralPath '.github' -Destination $package -Recurse -Force }
Copy-Item -LiteralPath 'dist\UFC6ActionsOverlaySetup.exe' -Destination (Join-Path $package 'UFC6ActionsOverlaySetup.exe')
New-Item -ItemType Directory -Force -Path "$package\tests" | Out-Null
Get-ChildItem -LiteralPath 'tests' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination "$package\tests"
}
Copy-Item -LiteralPath 'dist\libwinpthread-1.dll','dist\version.dll','dist\release.json','dist\diagnostics-components.json','dist\ufc6_menu_layer.dll','dist\ufc6_menu_layer.json','dist\ufc6_menu_restart.exe','dist\ufc6_menu_icons.exe','dist\ufc6_menu_update.exe' -Destination "$package\dist"
New-Item -ItemType Directory -Force -Path "$package\vendor\vulkan-headers" | Out-Null
Copy-Item -LiteralPath 'vendor\vulkan-headers\include','vendor\vulkan-headers\LICENSES' -Destination "$package\vendor\vulkan-headers" -Recurse -Force
Copy-Item -LiteralPath 'vendor\vulkan-headers\LICENSE.md' -Destination "$package\vendor\vulkan-headers"
New-Item -ItemType Directory -Force -Path "$package\vendor\xbox-icons\tests" | Out-Null
Get-ChildItem -LiteralPath 'vendor/xbox-icons' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination "$package\vendor\xbox-icons" }
Copy-Item -LiteralPath 'vendor/xbox-icons/profiles','vendor/xbox-icons/runtime-licenses' -Destination "$package\vendor\xbox-icons" -Recurse -Force
Get-ChildItem -LiteralPath 'vendor/xbox-icons/tests' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination "$package\vendor\xbox-icons\tests" }
# Exclude Python bytecode produced by the helper tests or a prior local build.
$packageRoot = [IO.Path]::GetFullPath($package).TrimEnd('\') + '\'
Get-ChildItem -LiteralPath $package -Directory -Recurse -Filter '__pycache__' | ForEach-Object {
    $cachePath = [IO.Path]::GetFullPath($_.FullName)
    if (-not $cachePath.StartsWith($packageRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Cache path is outside the release package.' }
    Remove-Item -LiteralPath $cachePath -Recurse -Force
}
$archive = Join-Path $PSScriptRoot "release\UFC6-Actions-Overlay-$version.zip"
Compress-Archive -LiteralPath $package -DestinationPath $archive -Force
Get-Item -LiteralPath $archive | Select-Object FullName,Length | Format-List
