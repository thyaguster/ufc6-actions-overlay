param(
    [Parameter(Mandatory=$true)][ValidateSet('Install','Remove')][string]$Action,
    [Parameter(Mandatory=$true)][string]$NovoFolder
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=[IO.Path]::GetFullPath($NovoFolder).TrimEnd('\')
$exe=Join-Path $folder 'UFC 6 novo.exe'
$expected='ACE7A59392B47A776D469768F4ED1AEF2C3395C26AC4ED57FD67661C2D6ABE48'
if(-not (Test-Path -LiteralPath $exe -PathType Leaf) -or (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $expected){
    throw 'The selected new engine does not match the verified PORTHEON build.'
}
if((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked engine folders are not supported.'}
$names=@('winmm.dll','version.dll','ufc6_menu_layer.dll','ufc6_menu_layer.json')
$manifest=Join-Path $root '.build\new-engine-probe.json'
$backup=Join-Path $root '.build\new-engine-probe-originals'
if($Action -eq 'Install'){
    if(Test-Path -LiteralPath $manifest){throw 'This test is already installed. Remove it before reinstalling.'}
    foreach($name in $names){
        $source=Join-Path $root "dist\$name"
        if(-not(Test-Path -LiteralPath $source -PathType Leaf)){throw "Missing test component: $name"}
        $dest=Join-Path $folder $name
        if((Test-Path -LiteralPath $dest) -and ((Get-Item -LiteralPath $dest).Attributes -band [IO.FileAttributes]::ReparsePoint)){
            throw "Linked component is not supported: $name"
        }
    }
    New-Item -ItemType Directory -Force -Path $backup | Out-Null
    $rows=@()
    foreach($name in $names){
        $source=Join-Path $root "dist\$name";$dest=Join-Path $folder $name;$original=Test-Path -LiteralPath $dest
        if($original){Copy-Item -LiteralPath $dest -Destination (Join-Path $backup $name) -Force}
        $rows+=@{name=$name;hadOriginal=[bool]$original;installedHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash}
    }
    try{
        foreach($name in $names){Copy-Item -LiteralPath (Join-Path $root "dist\$name") -Destination (Join-Path $folder $name) -Force}
        @{folder=$folder;files=$rows} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifest -Encoding UTF8
    }catch{
        foreach($row in $rows){
            $dest=Join-Path $folder $row.name
            if($row.hadOriginal){Copy-Item -LiteralPath (Join-Path $backup $row.name) -Destination $dest -Force}
            elseif(Test-Path -LiteralPath $dest){Remove-Item -LiteralPath $dest -Force}
        }
        throw
    }
    Write-Output "Installed reversible new-engine probe in $folder"
}else{
    if(-not(Test-Path -LiteralPath $manifest)){throw 'No probe manifest found.'}
    $state=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if([IO.Path]::GetFullPath($state.folder).TrimEnd('\') -ne $folder){throw 'Manifest points to a different engine folder.'}
    foreach($row in $state.files){
        $dest=Join-Path $folder $row.name
        if(-not(Test-Path -LiteralPath $dest -PathType Leaf) -or (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash -ne $row.installedHash){
            throw "Component changed since installation: $($row.name). Nothing was removed."
        }
        if($row.hadOriginal -and -not(Test-Path -LiteralPath (Join-Path $backup $row.name))){throw "Original backup missing: $($row.name)"}
    }
    foreach($row in $state.files){
        $dest=Join-Path $folder $row.name
        if($row.hadOriginal){Copy-Item -LiteralPath (Join-Path $backup $row.name) -Destination $dest -Force}
        else{Remove-Item -LiteralPath $dest -Force}
    }
    Remove-Item -LiteralPath $manifest -Force
    Write-Output "Restored new-engine probe files in $folder"
}
