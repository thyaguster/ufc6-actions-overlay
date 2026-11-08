"""Verify the real Windows screenshot destination, then remove only our test PNG."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import subprocess
import uuid
from PIL import Image

class GUID(c.Structure):
    _fields_=[("a",w.DWORD),("b",w.WORD),("c",w.WORD),("d",w.BYTE*8)]

shell=c.WinDLL("shell32",use_last_error=True)
shell.SHGetKnownFolderPath.argtypes=[c.POINTER(GUID),w.DWORD,w.HANDLE,c.POINTER(c.c_wchar_p)]
shell.SHGetKnownFolderPath.restype=c.c_long
ole=c.WinDLL("ole32")
ole.CoTaskMemFree.argtypes=[c.c_void_p]

def known_folder(value):
    guid=GUID.from_buffer_copy(uuid.UUID(value).bytes_le)
    text=c.c_wchar_p()
    # Resolve without requiring or creating a directory before the writer runs.
    result=shell.SHGetKnownFolderPath(c.byref(guid),0x4000,None,c.byref(text))
    if result<0:return None
    try:return Path(text.value)
    finally:ole.CoTaskMemFree(text)

folder=known_folder("b7bede81-df94-4682-a7d8-57a52620b86f")
if folder is None:
    pictures=known_folder("33e28130-4e1e-4676-835a-98395c3bc3bb")
    if pictures is None:raise RuntimeError("Windows Pictures could not be resolved")
    folder=pictures/"Screenshots"
root=Path(__file__).resolve().parents[1]
fixture=root/".build/vulkan-smoke"
before=set(folder.glob("UFC6_*.png"))
old_before=set((fixture/"Screenshots").glob("UFC6_*.png"))
subprocess.run([str(fixture/"vulkan_smoke.exe")],cwd=fixture,check=True,timeout=20,creationflags=0x08000000)
created=set(folder.glob("UFC6_*.png"))-before
assert len(created)==1,f"Expected one PNG in {folder}; found {len(created)}"
assert set((fixture/"Screenshots").glob("UFC6_*.png"))==old_before,"Writer still used the old game-local folder"
path=created.pop()
with Image.open(path) as screenshot:
    image=screenshot.convert("RGBA")
    assert image.size==(1008,729)
    assert image.getcolors(image.width*image.height)==[(image.width*image.height,(10,15,23,255))],"Overlay pixels entered the screenshot"
print(f"PASS: PNG saved to the Windows Screenshots known folder: {folder}")
print("PASS: full game pixels verified; no overlay included.")
path.unlink()
print("Removed only the generated test PNG; existing screenshots were untouched.")
