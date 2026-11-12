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
 core.MenuTestStart(window);pump();pump();core.Ufc6VulkanActive();core.Ufc6ScreenshotCapability(1)
 core.MenuTestKey.argtypes=[c.c_int,c.c_int]
 core.MenuTestKey(0x11,1);core.MenuTestKey(0x10,1);core.MenuTestKey(0x7b,1);pump()
 assert core.Ufc6ScreenshotRequest()==2;assert core.Ufc6ScreenshotRequest()==0;pump();assert core.Ufc6ScreenshotRequest()==0
 core.Ufc6ScreenshotResult(0);core.MenuTestKey(0x7b,0);core.MenuTestKey(0x10,0);core.MenuTestKey(0x11,0);pump()
 core.MenuTestInvokeBinding(1);pump();assert core.Ufc6ScreenshotRequest()==1;core.Ufc6ScreenshotResult(0)
 print('PASS: default Ctrl+Shift+F12 requests overlay capture once per hold; existing screenshot action remains clean.')
finally:u.DestroyWindow(window)
