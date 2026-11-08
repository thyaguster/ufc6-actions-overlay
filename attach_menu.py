"""Load the verified menu into the matching running engine, without a restart.

The DLL stays loaded until the game exits. It is never unloaded live.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path

from manage_menu import ROOT, DEFAULT_BIN, ENGINE_SHA256, digest

k = c.WinDLL("kernel32", use_last_error=True)
SIZE_T = c.c_size_t


class Module(c.Structure):
    _fields_ = [("dwSize", w.DWORD), ("th32ModuleID", w.DWORD),
                ("th32ProcessID", w.DWORD), ("GlblcntUsage", w.DWORD),
                ("ProccntUsage", w.DWORD), ("modBaseAddr", c.c_void_p),
                ("modBaseSize", w.DWORD), ("hModule", w.HMODULE),
                ("szModule", w.WCHAR * 256), ("szExePath", w.WCHAR * 260)]


def api(name, restype, *args):
    f = getattr(k, name)
    f.restype, f.argtypes = restype, list(args)
    return f


snapshot = api("CreateToolhelp32Snapshot", w.HANDLE, w.DWORD, w.DWORD)
first = api("Module32FirstW", w.BOOL, w.HANDLE, c.POINTER(Module))
next_module = api("Module32NextW", w.BOOL, w.HANDLE, c.POINTER(Module))
close = api("CloseHandle", w.BOOL, w.HANDLE)
open_process = api("OpenProcess", w.HANDLE, w.DWORD, w.BOOL, w.DWORD)
query_path = api("QueryFullProcessImageNameW", w.BOOL, w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD))
module_handle = api("GetModuleHandleW", w.HMODULE, w.LPCWSTR)
get_proc = api("GetProcAddress", c.c_void_p, w.HMODULE, w.LPCSTR)
allocate = api("VirtualAllocEx", c.c_void_p, w.HANDLE, c.c_void_p, SIZE_T, w.DWORD, w.DWORD)
free = api("VirtualFreeEx", w.BOOL, w.HANDLE, c.c_void_p, SIZE_T, w.DWORD)
write = api("WriteProcessMemory", w.BOOL, w.HANDLE, c.c_void_p, c.c_void_p, SIZE_T, c.POINTER(SIZE_T))
thread = api("CreateRemoteThread", w.HANDLE, w.HANDLE, c.c_void_p, SIZE_T, c.c_void_p, c.c_void_p, w.DWORD, c.POINTER(w.DWORD))
wait = api("WaitForSingleObject", w.DWORD, w.HANDLE, w.DWORD)


def modules(pid):
    h = snapshot(0x18, pid)
    if h == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    try:
        m = Module()
        m.dwSize = c.sizeof(m)
        if not first(h, c.byref(m)):
            raise c.WinError(c.get_last_error())
        result = []
        while True:
            result.append((m.szModule, m.szExePath, m.modBaseAddr, m.modBaseSize))
            if not next_module(h, c.byref(m)):
                if c.get_last_error() != 18:
                    raise c.WinError(c.get_last_error())
                return result
    finally:
        close(h)


def attach(pid, bin_dir):
    dll = (bin_dir / "version.dll").resolve(strict=True)
    expected = json.loads((ROOT / "dist/release.json").read_text(encoding="utf-8"))["dll_sha256"]
    if digest(dll) != expected:
        raise RuntimeError("Installed DLL does not match the verified build")
    h = open_process(0x2 | 0x8 | 0x10 | 0x20 | 0x400 | 0x100000, False, pid)
    if not h:
        raise c.WinError(c.get_last_error())
    remote = None
    may_free = True
    try:
        path_buffer, size = c.create_unicode_buffer(32768), w.DWORD(32768)
        if not query_path(h, 0, path_buffer, c.byref(size)):
            raise c.WinError(c.get_last_error())
        engine = Path(path_buffer.value).resolve(strict=True)
        if engine != (bin_dir / "UFC 6.exe").resolve(strict=True) or digest(engine) != ENGINE_SHA256:
            raise RuntimeError("Target process is not the supported installed UFC 6 engine")
        loaded = modules(pid)
        if any(Path(path).resolve() == dll for _, path, _, _ in loaded):
            print("Menu DLL is already loaded; restart the game to replace it.")
            return
        address = get_proc(module_handle("kernel32.dll"), b"LoadLibraryW")
        owner = next(m for m in modules(k.GetCurrentProcessId()) if m[2] <= address < m[2] + m[3])
        remote_owner = next(m for m in loaded if m[0].casefold() == owner[0].casefold() and m[3] == owner[3])
        remote_loader = remote_owner[2] + address - owner[2]
        data = (str(dll) + "\0").encode("utf-16-le")
        remote = allocate(h, None, len(data), 0x3000, 0x04)
        if not remote:
            raise c.WinError(c.get_last_error())
        copied = SIZE_T()
        if not write(h, remote, data, len(data), c.byref(copied)) or copied.value != len(data):
            raise c.WinError(c.get_last_error())
        t = thread(h, None, 0, remote_loader, remote, 0, None)
        if not t:
            raise c.WinError(c.get_last_error())
        may_free = False
        try:
            if wait(t, 10000) != 0:
                raise RuntimeError("DLL loading has not completed. Its small path buffer was retained; do not retry before checking the process.")
            may_free = True
        finally:
            close(t)
        if not any(Path(path).resolve() == dll for _, path, _, _ in modules(pid)):
            raise RuntimeError("Windows did not load the DLL; check dependencies")
        print(f"Menu DLL loaded into UFC 6 process {pid}. It remains loaded until that process exits.")
    finally:
        if remote and may_free:
            free(h, remote, 0, 0x8000)
        close(h)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--bin-dir", type=Path, default=DEFAULT_BIN)
    args = parser.parse_args()
    attach(args.pid, args.bin_dir)
