"""Install/remove verified menu files beside Kyty; never replace the engine or settings."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import uuid

ROOT=Path(__file__).resolve().parent
ENGINE_SHA256="f3c42126bd9df5c12cb69c1c57233a94ba024c50929ba0dfe20e715521f691a2"
DEFAULT_BIN=Path.cwd()/"bin"


def digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source,"sha256").hexdigest()


def manage(action: str,bin_dir: Path,payload: Path,manifest: Path) -> dict:
    bin_dir=bin_dir.resolve(strict=True)
    release=json.loads(manifest.read_text(encoding="utf-8"))
    companions=release.get("companion_files",{})
    # Commit the core last, after the new companions are in place.
    expected={**companions,"version.dll":release["dll_sha256"]}
    runtime=release.get("runtime_bridge")
    if runtime:
        expected={runtime["original_name"]:runtime["original_sha256"],**expected}
    previous={name:set(values) for name,values in release.get("previous_companion_sha256",{}).items()}
    previous["version.dll"]=set(release.get("previous_dll_sha256",[]))
    if runtime:previous.setdefault(runtime["target"],set()).add(runtime["original_sha256"])
    actual={}
    state={}
    for name,value in expected.items():
        if Path(name).name!=name:
            raise RuntimeError("Invalid companion filename in manifest")
        path=bin_dir/name
        if path.is_symlink():
            raise RuntimeError(f"Refusing symbolic link: {path}")
        actual[name]=digest(path) if path.exists() else None
        state[name]="original runtime" if runtime and name==runtime["target"] and actual[name]==runtime["original_sha256"] else "installed" if actual[name]==value else "installed (older build)" if actual[name] in previous.get(name,set()) else "conflict" if actual[name] else "absent"
    engine_hash=digest(bin_dir/"UFC 6.exe")
    result={"bin_dir":str(bin_dir),"engine_sha256":engine_hash,"supported_engine":engine_hash==ENGINE_SHA256,
            "menu":"conflicting DLL" if state["version.dll"]=="conflict" else state["version.dll"]}
    if companions:
        result["vulkan_files"]={name:state[name] for name in companions}
    parsec=bin_dir/"UFC 6 Parsec.exe"
    if release.get("parsec_engine_sha256") and parsec.exists():result["parsec_engine_supported"]=not parsec.is_symlink() and digest(parsec)==release["parsec_engine_sha256"]
    if action=="status":
        return result
    if state["version.dll"]=="conflict":
        raise RuntimeError("A different version.dll is present. It was left untouched; plugins using this filename cannot be combined automatically.")
    if any(state[name]=="conflict" for name in expected if name!="version.dll"):
        raise RuntimeError("A companion filename contains an unknown file. Nothing was changed.")
    if action!="remove" and engine_hash!=ENGINE_SHA256:
        raise RuntimeError("Engine hash does not match the supported PORTHEON 3309ab23f build. Nothing was changed.")
    original=None
    if runtime:
        source=next((bin_dir/name for name in [runtime["target"],runtime["original_name"]] if actual[name]==runtime["original_sha256"]),None)
        if source is None:raise RuntimeError("Verified original threading runtime is missing. Nothing was changed.")
        original=source.read_bytes()
        if hashlib.sha256(original).hexdigest()!=runtime["original_sha256"]:raise RuntimeError("Original runtime changed. Nothing was changed.")
    desired={name:((runtime["original_sha256"] if runtime and name==runtime["target"] else None) if action=="remove" else value) for name,value in expected.items()}
    content={}
    for name,value in desired.items():
        if value is None:continue
        source=payload if name=="version.dll" else payload.parent/name
        content[name]=original if runtime and (name==runtime["original_name"] or action=="remove") else source.read_bytes()
        if hashlib.sha256(content[name]).hexdigest()!=value:
            raise RuntimeError("Menu DLL does not match release.json. Nothing was changed." if name=="version.dll" else f"Companion hash mismatch: {name}. Nothing was changed.")
    order=list(reversed(expected)) if action=="remove" else list(expected)
    temps={}
    backups={}
    changed=[]
    try:
        for name in order:
            value=desired[name]
            if actual[name]==value:continue
            target=bin_dir/name
            if actual[name]:
                kind="removed" if action=="remove" else "previous"
                backup=bin_dir/f"ufc6-quit-menu.{kind}-{target.stem}-{actual[name][:16]}{target.suffix}"
                if backup.exists():backup=backup.with_name(backup.stem+"-"+uuid.uuid4().hex+backup.suffix)
                backups[name]=backup
            if value is not None:
                with tempfile.NamedTemporaryFile(dir=bin_dir,prefix=".ufc6-menu-",suffix=".tmp",delete=False) as temp:
                    temps[name]=Path(temp.name)
                    temp.write(content[name]);temp.flush();os.fsync(temp.fileno())
                if digest(temps[name])!=value:raise RuntimeError("Temporary copy failed verification.")
        for name in order:
            if name not in temps and name not in backups:continue
            target=bin_dir/name
            current=digest(target) if target.exists() else None
            if current!=actual[name] or target.is_symlink():raise RuntimeError(f"{name} changed during update; refusing to replace it.")
            changed.append(name)
            if name in backups:target.rename(backups[name])
            # Windows rename refuses an existing destination, preserving concurrent installs.
            if name in temps:
                temps[name].rename(target)
                if digest(target)!=desired[name]:raise RuntimeError("Installed file failed verification.")
    except Exception:
        for name in reversed(changed):
            target=bin_dir/name
            if name in temps and target.exists() and digest(target)==desired[name]:target.unlink()
            if name in backups and backups[name].exists() and not target.exists():backups[name].rename(target)
        raise
    finally:
        for temp in temps.values():temp.unlink(missing_ok=True)
    if action=="remove":
        for path in backups.values():path.unlink(missing_ok=True)
        result["menu"]="absent"
        if companions:result["vulkan_files"]={name:("original runtime restored" if runtime and name==runtime["target"] else "absent") for name in companions}
        return result
    result["menu"]="installed"
    if companions:result["vulkan_files"]={name:"installed" for name in companions}
    if "version.dll" in backups:result["previous_dll_backup"]=str(backups["version.dll"])
    saved={name:str(path) for name,path in backups.items() if name!="version.dll"}
    if saved:result["previous_companion_backups"]=saved
    if changed:result["activation"]="next game launch; already loaded modules remain active"
    return result


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action",choices=["status","install","remove"])
    parser.add_argument("--bin-dir",type=Path,default=DEFAULT_BIN)
    args=parser.parse_args()
    try:print(json.dumps(manage(args.action,args.bin_dir,ROOT/"dist/version.dll",ROOT/"dist/release.json"),indent=2))
    except (OSError,RuntimeError,ValueError) as exc:parser.exit(1,f"{exc}\n")


if __name__=="__main__":main()
