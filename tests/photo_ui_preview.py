"""Exercise icon selection through the real DLL and render both profiles offscreen."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import time

parser=argparse.ArgumentParser()
parser.add_argument("folder",type=Path)
parser.add_argument("--gallery",action="store_true")
parser.add_argument("--reload",action="store_true")
parser.add_argument("--reset",action="store_true")
parser.add_argument("--power",action="store_true")
parser.add_argument("--reboot",action="store_true")
parser.add_argument("--auto-launch",action="store_true")
parser.add_argument("--reload-auto-off",action="store_true")
parser.add_argument("--scale",action="store_true")
parser.add_argument("--guide",action="store_true")
parser.add_argument("--gamebar",action="store_true")
parser.add_argument("--conflict",action="store_true")
parser.add_argument("--no-render",action="store_true")
parser.add_argument("--reload-guide-off",action="store_true")
parser.add_argument("--controllers",action="store_true")
parser.add_argument("--raw-controllers",action="store_true")
parser.add_argument("--access",action="store_true")
parser.add_argument("--reload-access",action="store_true")
parser.add_argument("--raw-access",action="store_true")
parser.add_argument("--screenshot-reset",action="store_true")
parser.add_argument("--cheats",action="store_true")
parser.add_argument("--game-icons",action="store_true")
parser.add_argument("--longevity-save",action="store_true")
parser.add_argument("--longevity-reload",action="store_true")
parser.add_argument("--longevity-reload-off",action="store_true")
parser.add_argument("--training-save",action="store_true")
parser.add_argument("--training-reload",action="store_true")
parser.add_argument("--training-reload-off",action="store_true")
parser.add_argument("--bindings",action="store_true")
parser.add_argument("--bindings-reload",action="store_true")
parser.add_argument("--appearance",action="store_true")
parser.add_argument("--appearance-reload",action="store_true")
parser.add_argument("--window",action="store_true")
parser.add_argument("--window-reload",action="store_true")
parser.add_argument("--startup",action="store_true")
parser.add_argument("--startup-reload",action="store_true")
args=parser.parse_args()
folder=args.folder.resolve()
if args.reset:
    (folder/"ufc6-quit-menu.test.ini").write_text("[Menu]\nButtonIcons=Xbox\nOpeningShortcut=0xc08\nUIScale=100\n[Photo]\nWatermarkText=Photo Creator\n")
u=c.WinDLL("user32",use_last_error=True)
k=c.WinDLL("kernel32",use_last_error=True)
u.CreateWindowExW.argtypes=[w.DWORD,w.LPCWSTR,w.LPCWSTR,w.DWORD,c.c_int,c.c_int,c.c_int,c.c_int,w.HWND,w.HMENU,w.HINSTANCE,c.c_void_p]
u.CreateWindowExW.restype=w.HWND
k.GetModuleHandleW.argtypes=[w.LPCWSTR];k.GetModuleHandleW.restype=w.HMODULE
u.PeekMessageW.argtypes=[c.POINTER(w.MSG),w.HWND,w.UINT,w.UINT,w.UINT]
u.DispatchMessageW.argtypes=[c.POINTER(w.MSG)];u.DispatchMessageW.restype=w.LPARAM
u.TranslateMessage.argtypes=[c.POINTER(w.MSG)]
u.DestroyWindow.argtypes=[w.HWND]


def pump(seconds=.2):
    until=time.monotonic()+seconds
    msg=w.MSG()
    while time.monotonic()<until:
        while u.PeekMessageW(c.byref(msg),None,0,0,1):
            u.TranslateMessage(c.byref(msg));u.DispatchMessageW(c.byref(msg))
        time.sleep(.005)


window=u.CreateWindowExW(0,"STATIC","Hidden icon preview host",0,0,0,570,372,None,None,k.GetModuleHandleW(None),None)
if not window:raise c.WinError(c.get_last_error())
dll=c.CDLL(str(folder/"quit_menu_test.dll"))
dll.MenuTestStart.argtypes=[w.HWND];dll.MenuTestStart.restype=None
dll.MenuTestButton.argtypes=[w.DWORD,c.c_int];dll.MenuTestButton.restype=None
dll.MenuTestToggle.restype=None
dll.MenuTestSnapshot.restype=c.c_int
dll.MenuTestShortcut.restype=w.DWORD
dll.MenuTestShutdownRemaining.restype=c.c_int
dll.MenuTestScale.restype=c.c_int
dll.Ufc6MenuExtent.argtypes=[w.DWORD,w.DWORD,c.POINTER(w.DWORD),c.POINTER(w.DWORD)]
dll.Ufc6MenuExtent.restype=None
dll.Ufc6RenderMenu.argtypes=[c.c_void_p,w.DWORD];dll.Ufc6RenderMenu.restype=w.BOOL


def press(button):
    if not (args.startup or args.startup_reload or args.appearance or args.appearance_reload or args.window or args.window_reload) and button==0x4000 and ((dll.MenuTestSnapshot()>>12)&31)==0 and (dll.MenuTestSnapshot()&15)==5:
        dll.MenuTestLegacySettings();pump(.08);return
    dll.MenuTestButton(button,1);dll.MenuTestButton(button,0);pump(.06)



def render(name,viewport=(2560,1440)):
    if args.no_render:return
    from PIL import Image
    width,height=w.DWORD(),w.DWORD()
    dll.Ufc6MenuExtent(*viewport,c.byref(width),c.byref(height))
    pixels=c.create_string_buffer(width.value*height.value*4)
    dll.MenuTestTextClips(1)
    assert dll.Ufc6RenderMenu(pixels,len(pixels)),"Real menu renderer was not ready"
    assert dll.MenuTestTextClips(0)==0,"A rendered text line exceeds its rectangle height"
    image=Image.frombytes("RGBA",(width.value,height.value),pixels.raw,"raw","BGRA")
    image.save(folder/name)


try:
    dll.MenuTestStart(window);pump(.4);dll.Ufc6VulkanActive();dll.MenuTestToggle();pump(.2)
    render("main-photo-mode.png")
    for _ in range(4):press(0x40)
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==21
    render("photo-mode.png")
    for _ in range(8):press(0x40)
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==22
    render("photo-controls.png")
    for _ in range(11):press(0x40)
    render("photo-controls-scrolled.png")
    press(0x2000);assert (dll.MenuTestSnapshot()>>12)&31==21
    press(0x40);press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==23
    render("photo-filters.png")
    for _ in range(8):press(0x40)
    render("photo-effects.png")
    press(0x2000);press(0x40);press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==24
    render("photo-watermark.png")
    press(0x2000)
    press(0x2000);assert (dll.MenuTestSnapshot()>>12)&31==0
    help_panel=dll.MenuTestPhotoHelp;help_panel.argtypes=[c.c_int,c.c_int,c.c_int]
    dll.Ufc6OverlayBlend.restype=w.BOOL
    for ps in (0,1):
        help_panel(1,ps,ps);pump(.1)
        assert dll.Ufc6OverlayBlend()
        render("photo-help-playstation.png" if ps else "photo-help-xbox.png")
        width,height=w.DWORD(),w.DWORD();dll.Ufc6MenuExtent(2560,1440,c.byref(width),c.byref(height))
        assert width.value==440*dll.MenuTestScale()//100 and height.value==552*dll.MenuTestScale()//100
        pixels=c.create_string_buffer(width.value*height.value*4);assert dll.Ufc6RenderMenu(pixels,len(pixels))
        alpha=pixels.raw[3::4];assert min(alpha)==184 and max(alpha)==255
        x,y=w.DWORD(),w.DWORD();dll.Ufc6OverlayPosition(2560,1440,c.byref(x),c.byref(y));assert (x.value,y.value)==(24,259)
        dll.Ufc6MenuExtent(320,240,c.byref(width),c.byref(height));assert width.value<=288 and height.value<=208
        panel=dll.MenuTestPhotoPanel;panel.argtypes=[w.DWORD,w.DWORD];panel.restype=c.c_int
        assert panel(8,0)==1;render("photo-live-effects-ps.png" if ps else "photo-live-effects-xbox.png")
        assert panel(8,0)==2;render("photo-live-watermark-ps.png" if ps else "photo-live-watermark-xbox.png")
        assert panel(8,0)==0

    help_panel(0,0,0);pump(.1);assert not dll.Ufc6OverlayBlend()
    dll.MenuTestSettingsMode(0);dll.MenuTestInvokeBinding(3);pump(.15)
    for _ in range(3):press(0x40)
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==20
    press(0x40);render("screenshot-settings.png")
    press(0x4000);assert dll.MenuTestScreenshotUI()==1;render("screenshot-settings-on.png")
    press(0x4000);assert dll.MenuTestScreenshotUI()==0
    press(0x2000);press(0x2000)
    dll.MenuTestInvokeBinding(3);pump(.15)
    for _ in range(4):press(0x40)
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==25
    render("performance.png")
    press(0x4000);render("performance-unsaved.png")
    press(0x2000);assert (dll.MenuTestSnapshot()>>12)&31==27;render("performance-discard.png")
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==25
    press(0x4000);press(0x40);press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==16
    assert dll.MenuTestPerformanceSaves()==0
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==25
    press(0x40);press(0x4000);press(0x40);press(0x4000)
    for _ in range(3):press(0x40)
    press(0x4000);assert dll.MenuTestPerformanceSaves()==1;render("performance-saved.png")
    press(0x40);press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==26
    render("performance-restart-confirm.png")
    press(0x4000);assert dll.MenuTestPerformanceRestarts()==0
    press(0x4000);press(0x40);press(0x4000);assert dll.MenuTestPerformanceRestarts()==1 and dll.MenuTestPerformanceSaves()==2
    press(0x2000);press(0x2000)
    dll.MenuTestInvokeBinding(3);pump(.15)
    press(0x40);press(0x40);press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==19
    for _ in range(4):press(0x40)
    pump(5.2);render("startup-settings.png")
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==28;render("startup-screens.png")
    press(0x40);press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==29;render("startup-bypass-confirm.png")
    press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==28
    press(0x2000);assert (dll.MenuTestSnapshot()>>12)&31==19
    press(0x2000);press(0x2000)
    if args.gallery:
        dll.MenuTestSettingsMode(1);dll.MenuTestInvokeBinding(3);pump(.15)
        for _ in range(13):press(0x40)
        press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==12
        render("keybinds-photo-mode.png")
    print("PASS: Photo Mode/controls navigation and fonts; Xbox/PS help-panel artwork, frozen/live states, corner position, alpha coverage, small viewport fit and exit cleanup.")
finally:u.DestroyWindow(window)
