"""Exercise icon selection through the real DLL and render both profiles offscreen."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import time

parser=argparse.ArgumentParser()
parser.add_argument("folder",type=Path)
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
args=parser.parse_args()
folder=args.folder.resolve()
if args.reset:
    (folder/"ufc6-quit-menu.test.ini").write_text("[Menu]\nButtonIcons=Xbox\nOpeningShortcut=0xc08\nUIScale=100\n")
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
    dll.MenuTestButton(button,1);dll.MenuTestButton(button,0);pump(.06)


def render(name,viewport=(2560,1440)):
    if args.no_render:return
    from PIL import Image
    width,height=w.DWORD(),w.DWORD()
    dll.Ufc6MenuExtent(*viewport,c.byref(width),c.byref(height))
    pixels=c.create_string_buffer(width.value*height.value*4)
    assert dll.Ufc6RenderMenu(pixels,len(pixels)),"Real menu renderer was not ready"
    image=Image.frombytes("RGBA",(width.value,height.value),pixels.raw,"raw","BGRA")
    image.save(folder/name)


try:
    dll.MenuTestStart(window);pump(.4)
    assert (dll.MenuTestSnapshot()>>4)&1 == int(args.reload),"Stored profile was not loaded"
    assert dll.MenuTestScale()==(175 if args.reload and args.scale else 100)
    if args.reload:
        assert dll.MenuTestShortcut()==6,"Recorded shortcut was not persisted"
        dll.MenuTestButton(2,1);dll.MenuTestButton(4,1);pump(.95)
        dll.MenuTestButton(2,0);dll.MenuTestButton(4,0);pump(.15)
    else:
        dll.MenuTestToggle();pump(.3)
    assert dll.MenuTestSnapshot()&0x100
    if args.raw_controllers:
        dll.MenuTestRawPad.argtypes=[c.c_int,c.c_int,c.c_int]
        dll.MenuTestButtonDevice.argtypes=[c.c_int,w.DWORD,c.c_int]
        dll.MenuTestAxisDevice.argtypes=[c.c_int,c.c_int,c.c_int]
        def raw(slot,buttons,connected=1):dll.MenuTestRawPad(slot,buttons,connected);pump(.04)
        def raw_press(slot,buttons):raw(slot,buttons);raw(slot,0)
        press(0x2000)
        raw(1,0x1000) # A held on the other controller must not block Guide.
        raw(0,0x400);assert dll.MenuTestSnapshot()&0x100
        raw(0,0);raw_press(0,2)
        assert dll.MenuTestSnapshot()&0xf==1
        dll.MenuTestButtonDevice(1,0x40,1);dll.MenuTestButtonDevice(1,0x40,0);pump(.04)
        assert dll.MenuTestSnapshot()&0xf==1,"SDL duplicates must not navigate twice"
        raw(1,2);assert dll.MenuTestSnapshot()&0xf==1
        raw_press(0,2);raw_press(0,0x1000)
        assert (dll.MenuTestSnapshot()>>12)&7==2
        raw_press(0,0x2000);raw_press(0,0x2000)
        assert not dll.MenuTestSnapshot()&0x100
        raw(1,0);raw(1,0x400);raw(1,0);raw_press(1,2)
        assert dll.MenuTestSnapshot()&0xf==1,"Guide's own XInput controller must navigate without SDL events"
        raw(1,0,0);assert not dll.MenuTestSnapshot()&0x100
        raw(0,0x310)
        dll.MenuTestButtonDevice(1,0xc08,1);pump(.95)
        assert dll.MenuTestSnapshot()&0x100
        dll.MenuTestButtonDevice(1,0xc08,0);raw(0,0);raw_press(0,2)
        assert dll.MenuTestSnapshot()&0xf==1,"Xbox chord must use the same reliable polled navigation"
        raw_press(0,0x2000);assert not dll.MenuTestSnapshot()&0x100
        raw(0,0x400);raw(0,0)
        dll.MenuTestAxisDevice(2,1,255);pump(.06)
        assert dll.MenuTestSnapshot()&0xf==1,"SDL/HID stick navigation must work when XInput only reports Guide"
        dll.MenuTestAxisDevice(2,1,128);pump(.04)
        dll.MenuTestAxisDevice(2,1,255);pump(.04)
        assert dll.MenuTestSnapshot()&0xf==2
        dll.MenuTestAxisDevice(2,1,128);pump(.04)
        dll.MenuTestButtonDevice(2,0x2000,1);dll.MenuTestButtonDevice(2,0x2000,0);pump(.05)
        assert not dll.MenuTestSnapshot()&0x100
        raw(0,0x400);raw(0,0)
        dll.MenuTestButtonDevice(2,0x40,1);dll.MenuTestButtonDevice(2,0x40,0);pump(.05)
        assert dll.MenuTestSnapshot()&0xf==1,"SDL/HID D-pad must claim navigation when XInput only reports Guide"
        dll.MenuTestButtonDevice(2,0x2000,1);dll.MenuTestButtonDevice(2,0x2000,0);pump(.05)
        assert not dll.MenuTestSnapshot()&0x100
        print("PASS: direct XInput Guide navigation on either pad, no SDL input dependency/duplicates, page actions and disconnect cancellation.")
        raise SystemExit(0)
    elif args.controllers:
        dll.MenuTestButtonDevice.argtypes=[c.c_int,w.DWORD,c.c_int]
        dll.MenuTestGuide.argtypes=[c.c_int]
        dll.MenuTestDisconnect.argtypes=[c.c_int]
        dll.MenuTestOwner.restype=c.c_int
        press(0x2000) # Close initial menu.
        dll.MenuTestButtonDevice(2,0x4000,1);pump(.06) # Other pad holds A before Guide opens.
        dll.MenuTestGuide(1);pump(.08)
        assert dll.MenuTestSnapshot()&0x100 and dll.MenuTestOwner()==-1
        dll.MenuTestGuide(0);pump(.08)
        dll.MenuTestButtonDevice(2,0x20,0);pump(.04) # A release/no-op must not claim navigation.
        assert dll.MenuTestOwner()==-1
        dll.MenuTestButtonDevice(1,0x40,1);dll.MenuTestButtonDevice(1,0x40,0);pump(.08)
        assert dll.MenuTestSnapshot()&0xf==1,"Held buttons on another pad must not prevent Guide navigation"
        owner=dll.MenuTestOwner();assert owner>=0
        dll.MenuTestButtonDevice(2,0x40,1);dll.MenuTestButtonDevice(2,0x40,0);pump(.08)
        assert dll.MenuTestSnapshot()&0xf==1 and dll.MenuTestOwner()==owner
        dll.MenuTestButtonDevice(1,0x2000,1);dll.MenuTestButtonDevice(1,0x2000,0);pump(.08)
        assert not dll.MenuTestSnapshot()&0x100
        dll.MenuTestButtonDevice(2,0x4000,0);pump(.08)
        dll.MenuTestGuide(1);pump(.06);dll.MenuTestGuide(0);pump(.06)
        dll.MenuTestButtonDevice(2,0x40,1);dll.MenuTestButtonDevice(2,0x40,0);pump(.08)
        assert dll.MenuTestSnapshot()&0xf==1,"Second pad must be able to claim a fresh Guide-opened menu"
        dll.MenuTestButtonDevice(2,0x2000,1);dll.MenuTestButtonDevice(2,0x2000,0);pump(.08)
        assert not dll.MenuTestSnapshot()&0x100
        dll.MenuTestGuide(1);pump(.06);dll.MenuTestGuide(0);pump(.06)
        dll.MenuTestButtonDevice(1,0x40,1);dll.MenuTestButtonDevice(1,0x40,0);pump(.06)
        dll.MenuTestButtonDevice(1,0x400,1);pump(.04)
        dll.MenuTestDisconnect(1);pump(.08)
        assert not dll.MenuTestSnapshot()&0x100,"Disconnecting the owner must close the menu"
        dll.MenuTestGuide(1);pump(.06);dll.MenuTestGuide(0);pump(.06)
        for _ in range(3):
            dll.MenuTestButtonDevice(1,0x40,1);dll.MenuTestButtonDevice(1,0x40,0);pump(.05)
        dll.MenuTestButtonDevice(1,0x4000,1);dll.MenuTestButtonDevice(1,0x4000,0);pump(.07)
        assert (dll.MenuTestSnapshot()>>12)&7==1
        dll.MenuTestButtonDevice(1,0x4000,1);dll.MenuTestButtonDevice(1,0x4000,0);pump(.07)
        assert dll.MenuTestSnapshot()&0x10,"Reconnected pad must not inherit stale held buttons"
        print("PASS: Guide release, two-controller ownership, held secondary input, non-owner isolation, disconnect/reconnect and B resume.")
        raise SystemExit(0)
    elif args.conflict:
        dll.MenuTestRedirected.argtypes=[c.c_int]
        assert dll.MenuTestConflict()==0
        render("conflict-clear.png")
        dll.MenuTestRedirected(1);pump(.08)
        assert dll.MenuTestConflict()==1
        render("gamebar-conflict-xbox.png")
        press(0x40);assert dll.MenuTestSnapshot()&0xf==0,"Redirected controller must not activate menu actions"
        dll.MenuTestRedirected(0);pump(.08)
        assert dll.MenuTestConflict()==0
        for _ in range(3):press(0x40)
        press(0x4000);press(0x4000) # Settings, PlayStation prompts.
        press(0x40);press(0x4000) # Record custom LS + RS shortcut.
        dll.MenuTestButton(2,1);dll.MenuTestButton(4,1);pump(.4)
        dll.MenuTestButton(2,0);dll.MenuTestButton(4,0);pump(.08)
        assert dll.MenuTestShortcut()==6
        dll.MenuTestRedirected(1);pump(.08)
        render("gamebar-conflict-custom-playstation.png")
        dll.MenuTestToggle();pump(.08);assert not dll.MenuTestSnapshot()&0x100,"Keyboard fallback toggle must still close during conflict"
        assert not dll.MenuTestConflict()
        dll.MenuTestRedirected(0);pump(.08)
        dll.MenuTestButton(2,1);dll.MenuTestButton(4,1);pump(.95)
        dll.MenuTestButton(2,0);dll.MenuTestButton(4,0);pump(.08)
        assert dll.MenuTestSnapshot()&0x100,"Custom fallback still opens after Game Bar is dismissed"
        for _ in range(6):press(0x40)
        press(0x4000);press(0x40)
        dll.MenuTestButton(0x4000,1);pump(3.2)
        dll.MenuTestButton(0x4000,0);pump(.05)
        assert dll.MenuTestShutdownRemaining()>0
        dll.MenuTestRedirected(1);pump(.08)
        assert dll.MenuTestShutdownRemaining()==0,"Game Bar redirection must cancel power countdown"
        render("gamebar-conflict-power-cancelled.png")
        print("PASS: persistent warning/clear, blocked redirected input, custom PlayStation fallback, keyboard close, fallback reopening and power countdown cancellation.")
        raise SystemExit(0)
    elif args.gamebar:
        import winreg
        def windows_preference():
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r"Software\Microsoft\GameBar") as key:
                    return winreg.QueryValueEx(key,"UseNexusForGameBarEnabled")
            except FileNotFoundError:return None
        original_windows=windows_preference()
        for _ in range(3):press(0x40)
        press(0x4000)
        for _ in range(5):press(0x40)
        assert dll.MenuTestSnapshot()&0xf==5 and dll.MenuTestGameBar()==1
        render("gamebar-shortcut-on.png")
        press(0x4000);assert dll.MenuTestGameBar()==0
        render("gamebar-shortcut-off.png")
        press(0x80);assert dll.MenuTestGameBar()==1
        press(0x20);assert dll.MenuTestGameBar()==0
        press(0x40);assert dll.MenuTestSnapshot()&0xf==6
        press(0x40);assert dll.MenuTestSnapshot()&0xf==7
        render("settings-back.png")
        press(0x4000);assert (dll.MenuTestSnapshot()>>12)&7==0
        assert windows_preference()==original_windows,"UI test must never change the actual Windows Game Bar preference"
        print("PASS: Game Bar settings row, On/Off, confirm/left/right and Back; actual Windows preference unchanged.")
    elif args.guide or args.reload_guide_off:
        assert dll.MenuTestGuideEnabled()==(0 if args.reload_guide_off else 1)
        for _ in range(3):press(0x40)
        press(0x4000)
        for _ in range(4):press(0x40)
        assert dll.MenuTestSnapshot()&0xf==4
        render("guide-off-reloaded.png" if args.reload_guide_off else "guide-on.png")
        if not args.reload_guide_off:
            press(0x4000);assert dll.MenuTestGuideEnabled()==0
            render("guide-off.png")
            press(0x80);assert dll.MenuTestGuideEnabled()==1
            press(0x20);assert dll.MenuTestGuideEnabled()==0
            assert "GuideButtonOpensMenu=0" in (folder/"ufc6-quit-menu.test.ini").read_text()
        print("PASS: Guide setting defaults On, toggles and saves Off." if not args.reload_guide_off else "PASS: fresh process loaded Guide Off and rendered the saved setting.")
    elif args.auto_launch or args.reload_auto_off:
        assert dll.MenuTestAutoLaunch()==(0 if args.reload_auto_off else 1)
        render("main-auto-off.png" if args.reload_auto_off else "main-auto-on.png")
        if not args.reload_auto_off:
            for _ in range(3):press(0x40)
            press(0x4000)
            for _ in range(3):press(0x40)
            render("settings-auto-on.png")
            press(0x4000)
            assert dll.MenuTestAutoLaunch()==0
            assert "AutoLaunchAfterRestart=0" in (folder/"ufc6-quit-menu.test.ini").read_text()
            render("settings-auto-off.png")
            press(0x80);assert dll.MenuTestAutoLaunch()==1
            press(0x20);assert dll.MenuTestAutoLaunch()==0
            press(0x2000)
            press(0x40);press(0x40)
        else:
            for _ in range(5):press(0x40)
        render("main-auto-off.png")
        press(0x4000)
        render("confirmation-auto-off.png")
        assert (dll.MenuTestSnapshot()>>12)&7==4
        print("PASS: auto-launch defaults On, toggles with confirm/left/right, saves Off, and renders status on both pages." if not args.reload_auto_off else "PASS: a fresh process loaded auto-launch Off and showed it in the reboot confirmation.")
    elif args.power or args.reboot:
        render("main-menu.png")
        for _ in range(5 if args.reboot else 6):press(0x40)
        press(0x4000)
        assert (dll.MenuTestSnapshot()>>12)&7 == (4 if args.reboot else 3) and dll.MenuTestSnapshot()&0xf == 0
        render("reboot-confirmation.png" if args.reboot else "shutdown-confirmation.png")
        press(0x40)
        dll.MenuTestButton(0x4000,1);pump(1.5)
        render("reboot-hold.png" if args.reboot else "shutdown-hold.png")
        pump(1.7)
        assert 8<=dll.MenuTestShutdownRemaining()<=10
        render("reboot-countdown.png" if args.reboot else "shutdown-countdown.png")
        dll.MenuTestButton(0x4000,0);pump(.1);press(0x2000)
        assert dll.MenuTestShutdownRemaining()==0 and (dll.MenuTestSnapshot()>>12)&3 == 0
        print("PASS: shutdown confirmation, real 3-second hold, countdown and cancellation; no power operation called.")
    elif args.reload:
        render("playstation-reloaded.png")
        print("PASS: fresh process loaded PlayStation icons and opened using the saved custom shortcut.")
    else:
        render("main-menu.png")
        for _ in range(3):press(0x40)
        press(0x4000)  # Open the second settings box.
        assert (dll.MenuTestSnapshot()>>12)&3 == 1
        render("xbox-icons.png")
        press(0x4000)  # Confirm changes icons; it must not quit/resume.
        assert dll.MenuTestSnapshot()==0x1110
        render("playstation-icons.png")
        ini=(folder/"ufc6-quit-menu.test.ini").read_text()
        assert "ButtonIcons=PlayStation" in ini
        press(0x80)
        assert dll.MenuTestSnapshot()==0x1100
        render("xbox-setting.png")
        press(0x20)
        assert dll.MenuTestSnapshot()==0x1110
        assert "ButtonIcons=PlayStation" in (folder/"ufc6-quit-menu.test.ini").read_text()
        press(0x40);press(0x4000)
        assert dll.MenuTestSnapshot()&(1<<16)
        render("record-shortcut.png")
        dll.MenuTestButton(2,1);dll.MenuTestButton(4,1);pump(.45)
        dll.MenuTestButton(2,0);dll.MenuTestButton(4,0);pump(.2)
        assert not dll.MenuTestSnapshot()&(1<<16)
        assert dll.MenuTestShortcut()==6
        render("custom-shortcut.png")
        assert "OpeningShortcut=0x00000006" in (folder/"ufc6-quit-menu.test.ini").read_text()
        if args.scale:
            press(0x40)
            assert dll.MenuTestSnapshot()&0xf==2
            for _ in range(2):press(0x80)
            assert dll.MenuTestScale()==50;render("ui-scale-050.png")
            for _ in range(6):press(0x20)
            assert dll.MenuTestScale()==200;render("ui-scale-200.png")
            for _ in range(4):press(0x20)
            assert dll.MenuTestScale()==300;render("ui-scale-300.png")
            render("ui-scale-fit-640.png",(640,360))
            press(0x4000)
            assert dll.MenuTestScale()==100;render("ui-scale-reset.png")
            for _ in range(3):press(0x20)
            assert dll.MenuTestScale()==175;render("ui-scale-175.png")
            assert "UIScale=175" in (folder/"ufc6-quit-menu.test.ini").read_text()
            print("PASS: actual renderer resized from 50% to 300%, fit a small viewport, reset, and saved 175%.")
        print("PASS: separate settings, icon switching, shortcut recording, rendering, and persistence.")
    press(0x2000)
    if (dll.MenuTestSnapshot()>>12)&3 == 0 and dll.MenuTestSnapshot()&0x100:
        while dll.MenuTestSnapshot()&0xf != 2:press(0x40)
        press(0x4000)
        assert (dll.MenuTestSnapshot()>>12)&3 == 2 and dll.MenuTestSnapshot()&0xf == 0
        render("restart-confirmation.png")
        press(0x4000)  # Cancel is selected initially; it must keep the host alive.
        press(0x2000)
    assert not dll.MenuTestSnapshot()&0x100
    print("PASS: Back resumes from the setting without exiting the host.")
finally:
    u.DestroyWindow(window);pump(.1)
