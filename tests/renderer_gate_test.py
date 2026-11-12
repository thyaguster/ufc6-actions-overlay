"""A missing renderer must never leave an invisible menu owning game input."""
import ctypes as c,time,sys
from ctypes import wintypes as w
from pathlib import Path
u=c.WinDLL('user32');k=c.WinDLL('kernel32');u.CreateWindowExW.argtypes=[w.DWORD,w.LPCWSTR,w.LPCWSTR,w.DWORD,c.c_int,c.c_int,c.c_int,c.c_int,w.HWND,w.HMENU,w.HINSTANCE,c.c_void_p];u.CreateWindowExW.restype=w.HWND
k.GetModuleHandleW.argtypes=[w.LPCWSTR];k.GetModuleHandleW.restype=w.HMODULE;u.PeekMessageW.argtypes=[c.POINTER(w.MSG),w.HWND,w.UINT,w.UINT,w.UINT];u.DestroyWindow.argtypes=[w.HWND]
def pump():
 deadline=time.monotonic()+.2;msg=w.MSG()
 while time.monotonic()<deadline:
  while u.PeekMessageW(c.byref(msg),None,0,0,1):u.TranslateMessage(c.byref(msg));u.DispatchMessageW(c.byref(msg))
  time.sleep(.005)
window=u.CreateWindowExW(0,'STATIC','Renderer guard fixture',0,0,0,570,372,None,None,k.GetModuleHandleW(None),None);assert window
core=c.CDLL(str(Path(sys.argv[1]).resolve()/'quit_menu_test.dll'));core.MenuTestStart.argtypes=[w.HWND];core.MenuTestRendererGate.argtypes=[c.c_int];core.MenuTestGuide.argtypes=[c.c_int]
try:
 core.MenuTestStart(window);pump();pump();core.MenuTestRendererGate(1)
 core.MenuTestToggle();pump();assert not(core.MenuTestSnapshot()&256)
 core.MenuTestGuide(1);pump();assert not(core.MenuTestSnapshot()&256)
 core.MenuTestGuide(0);pump();core.Ufc6VulkanActive();core.MenuTestToggle();pump();assert core.MenuTestSnapshot()&256
 core.MenuTestToggle();pump();assert not(core.MenuTestSnapshot()&256)
 print('PASS: keyboard/Guide cannot open an invisible menu; normal opening/resume works when the renderer activates.')
finally:u.DestroyWindow(window)
