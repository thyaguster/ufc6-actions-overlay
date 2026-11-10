param([string]$DependencyPath)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$vendorRoot=Join-Path $PSScriptRoot 'vendor/xbox-icons'
$savedPythonPath=$env:PYTHONPATH
try {
    if($DependencyPath){$env:PYTHONPATH=$DependencyPath}
    python -m PyInstaller --noconfirm --clean --onefile --windowed --name ufc6_menu_icons --distpath dist --workpath .build/icons-build --specpath .build/icons-spec --paths $vendorRoot --add-data "$vendorRoot/profiles;profiles" --add-data "$vendorRoot/LICENSE;." --add-data "$vendorRoot/THIRD-PARTY-NOTICES.md;." --add-data "$vendorRoot/runtime-licenses;runtime-licenses" src/game_icons_helper.py
    if($LASTEXITCODE){throw 'Game-icons helper compilation failed. Install PyInstaller==6.22.3 and pyinstaller-hooks-contrib==2026.9.'}
} finally {$env:PYTHONPATH=$savedPythonPath}
