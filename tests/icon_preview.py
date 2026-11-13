"""Exercise icon selection through the real DLL and render both profiles offscreen."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import time

parser=argparse.ArgumentParser()
parser.add_argument("folder",type=Path)
parser.add_argument("--reload",action="store_true")
parser.add_argument("--decision-replays",action="store_true")
parser.add_argument("--decision-replays-reload",action="store_true")
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
    if not (args.decision_replays or args.decision_replays_reload or args.startup or args.startup_reload or args.appearance or args.appearance_reload or args.window or args.window_reload) and button==0x4000 and ((dll.MenuTestSnapshot()>>12)&31)==0 and (dll.MenuTestSnapshot()&15)==5:
        dll.MenuTestLegacySettings();pump(.08);return
    dll.MenuTestButton(button,1);dll.MenuTestButton(button,0);pump(.06)
    # These existing scenarios target pre-Photo actions; navigate past its new row.
    if button in (0x10,0x40) and ((dll.MenuTestSnapshot()>>12)&31)==0 and dll.MenuTestSnapshot()&15==4:
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
    dll.MenuTestStart(window);pump(.4)
    dll.MenuTestSettingsMode(0 if (args.decision_replays or args.decision_replays_reload or args.startup or args.startup_reload or args.appearance or args.appearance_reload or args.window or args.window_reload) else 1)
    dll.Ufc6VulkanActive() # Match production before opening; no desktop-fit/render extent race.
    assert (dll.MenuTestSnapshot()>>4)&1 == (1 if args.appearance_reload else int(args.reload)),"Stored profile was not loaded"
    assert dll.MenuTestScale()==(175 if args.reload and args.scale else 100)
    if args.reload:
        assert dll.MenuTestShortcut()==6,"Recorded shortcut was not persisted"
        dll.MenuTestButton(2,1);dll.MenuTestButton(4,1);pump(.95)
        dll.MenuTestButton(2,0);dll.MenuTestButton(4,0);pump(.15)
    else:
        dll.MenuTestToggle();pump(.3)
    assert dll.MenuTestSnapshot()&0x100
    if args.decision_replays or args.decision_replays_reload:
        k.GetPrivateProfileIntW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_int,w.LPCWSTR]
        def saved_replay():return k.GetPrivateProfileIntW("Menu","OfferDecisionReplays",0,str(folder/"ufc6-quit-menu.test.ini"))
        for _ in range(4):press(0x40)
        press(0x4000);press(0x40);press(0x40);press(0x4000)
        assert (dll.MenuTestSnapshot()>>12)&31==19
        for _ in range(7):press(0x40)
        assert dll.MenuTestSnapshot()&15==7
        assert saved_replay()==int(args.decision_replays_reload)
        render("decision-replays-loaded.png")
        press(0x4000)
        assert saved_replay()==int(not args.decision_replays_reload)
        render("decision-replays-toggled.png")
        print("PASS: decision replay switch navigation, persistence and unclipped rendering")
        raise SystemExit(0)
    if args.startup or args.startup_reload:
        k.GetPrivateProfileIntW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_int,w.LPCWSTR]
        def saved(name):return k.GetPrivateProfileIntW("Menu",name,0,str(folder/"ufc6-quit-menu.test.ini"))
        for _ in range(4):press(0x40)
        press(0x4000);press(0x40);press(0x40);press(0x4000)
        assert (dll.MenuTestSnapshot()>>12)&31==19
        for _ in range(3):press(0x40)
        if args.startup_reload:
            assert saved("SkipStartupSplashes")==1 and saved("StartupSkipPopups")==0
            # Turning Skip Off keeps the independent override Off.
            press(0x4000)
            assert saved("SkipStartupSplashes")==0 and saved("StartupSkipPopups")==0
            # A fresh On transition enables all three again and persists them.
            press(0x4000)
            assert saved("SkipStartupSplashes")==1 and saved("StartupSkipPopups")==1
            # An Off transition also disables an enabled auto-continue setting.
            press(0x4000)
            assert saved("SkipStartupSplashes")==0 and saved("StartupSkipPopups")==0
        else:
            press(0x4000)
            assert saved("SkipStartupSplashes")==1 and saved("StartupSkipPopups")==1
            press(0x40);press(0x40);press(0x4000)
            assert saved("SkipStartupSplashes")==1 and saved("StartupSkipPopups")==0
            pump(5.2);render("startup-options.png")
        print("PASS: experimental startup UI settings, font bounds and saved toggles; no game callbacks modified in test host.")
        raise SystemExit(0)
    elif args.window or args.window_reload:
        if args.window_reload:
            assert dll.MenuTestWindowSize()==2 and dll.MenuTestFontSize()==18
            print("PASS: taller window preference survives a fresh process.");raise SystemExit(0)
        def page():return (dll.MenuTestSnapshot()>>12)&31
        for _ in range(4):press(0x40)
        press(0x4000);press(0x4000)
        for _ in range(4):press(0x40)
        assert page()==17 and dll.MenuTestSnapshot()&15==4
        press(0x4000);assert dll.MenuTestWindowSize()==1 and dll.MenuTestVisibleRows()==7
        render("appearance-expanded.png")
        press(0x2000);press(0x2000);press(0x10);press(0x4000);assert page()==7
        assert dll.MenuTestVisibleRows()==9 and dll.MenuTestFontSize()==18 and dll.MenuTestScale()==100
        width,height=w.DWORD(),w.DWORD();dll.Ufc6MenuExtent(2560,1440,c.byref(width),c.byref(height));assert (width.value,height.value)==(570,468)
        render("cheats-expanded.png")
        press(0x2000);press(0x40);press(0x4000);press(0x4000)
        for _ in range(4):press(0x40)
        press(0x4000);assert dll.MenuTestWindowSize()==2
        press(0x2000);press(0x2000);press(0x10);press(0x4000)
        assert dll.MenuTestVisibleRows()==12 and dll.MenuTestFontSize()==18
        dll.Ufc6MenuExtent(2560,1440,c.byref(width),c.byref(height));assert (width.value,height.value)==(570,564)
        render("cheats-tall.png")
        for _ in range(11):press(0x40)
        render("cheats-tall-back.png")
        dll.Ufc6MenuExtent(640,360,c.byref(width),c.byref(height));assert width.value<=608 and height.value<=328
        render("cheats-tall-small-viewport.png",(640,360))
        print("PASS: expanded/tall windows show 9/12 Cheats rows with unchanged font/scale, selection reaches Back, viewport fitting and saved preference.")
        raise SystemExit(0)
    elif args.appearance or args.appearance_reload:
        if args.appearance_reload:
            assert dll.MenuTestFontSize()==20 and dll.MenuTestBoxStyle()==4 and dll.MenuTestThemeChoice()==2
            print("PASS: font size, row style and theme survive a fresh process.");raise SystemExit(0)
        def page():return (dll.MenuTestSnapshot()>>12)&31
        for _ in range(4):press(0x40)
        press(0x4000);assert page()==16;render("settings-categories.png")
        press(0x4000);assert page()==17;render("appearance.png")
        press(0x40);press(0x40);press(0x20);assert dll.MenuTestFontSize()==20
        render("appearance-large-font.png")
        press(0x40)
        for style,label in [(1,"square"),(2,"outline"),(3,"pill"),(4,"octagon")]:
            press(0x20);assert dll.MenuTestBoxStyle()==style;render("appearance-"+label+".png")
        for _ in range(3):press(0x10)
        press(0x4000);assert page()==14
        press(0x40);press(0x40);press(0x4000);press(0x2000);assert page()==17
        for _ in range(5):press(0x40)
        press(0x4000);assert (dll.MenuTestSnapshot()>>4)&1==1
        press(0x2000);assert page()==16
        press(0x40);press(0x4000);assert page()==18;render("controls.png")
        press(0x4000);assert page()==12
        press(0x40);press(0x4000);assert page()==13
        press(0x40);press(0x4000)
        dll.MenuTestButton(0x4800,1);pump(.4);dll.MenuTestButton(0x4800,0);pump(.15)
        assert dll.MenuTestBinding(1,0)==0x4800
        render("shortcut-ps-symbols.png")
        for _ in range(4):press(0x2000)
        assert page()==0;render("main-ps-icons-large-font.png")
        press(0x4000);press(0x4000)
        for _ in range(5):press(0x40)
        press(0x4000);assert not (dll.MenuTestSnapshot()>>4)&1
        press(0x2000);press(0x2000);render("main-xbox-icons-large-font.png")
        press(0x4000);press(0x4000)
        for _ in range(5):press(0x40)
        press(0x4000);press(0x2000);press(0x2000)

        # Visit every settings category and return without losing the group.
        for category,expected in [(2,19),(3,20)]:
            while dll.MenuTestSnapshot()&15!=5:press(0x40)
            press(0x4000)
            for _ in range(category):press(0x40)
            press(0x4000);assert page()==expected
            render("game-launcher.png" if category==2 else "files-support.png")
            press(0x2000);assert page()==16;press(0x2000)
        dll.MenuTestStatus.argtypes=[w.LPCWSTR];dll.MenuTestStatus("Screenshot saved")
        dll.MenuTestToggle();pump(.12);assert not dll.MenuTestSnapshot()&0x100
        render("notification-screenshot.png")
        x,y=w.DWORD(),w.DWORD();dll.Ufc6OverlayPosition.argtypes=[w.DWORD,w.DWORD,c.POINTER(w.DWORD),c.POINTER(w.DWORD)]
        dll.Ufc6OverlayPosition(2560,1440,c.byref(x),c.byref(y));assert x.value==2116 and y.value==24
        pump(3.1);assert not dll.Ufc6RenderMenu(None,0),"Notification must expire without opening or capturing the menu"
        print("PASS: grouped Settings routes/back, consistent large font text bounds, all box styles, PlayStation shortcut glyphs, persisted appearance and focus-free expiring corner notification.")
        raise SystemExit(0)
    elif args.bindings or args.bindings_reload:
        dll.MenuTestKey.argtypes=[c.c_int,c.c_int];dll.MenuTestBinding.argtypes=[c.c_int,c.c_int];dll.MenuTestBinding.restype=w.DWORD
        def page():return (dll.MenuTestSnapshot()>>12)&31
        def key(vk,down):dll.MenuTestKey(vk,down);pump(.08)
        if args.bindings_reload:
            assert dll.MenuTestBinding(1,1)==0x17b and dll.MenuTestBinding(1,0)==0x802
            assert dll.MenuTestBinding(0,0)==0xc08
            assert dll.MenuTestThemeChoice()==2 and dll.MenuTestThemeAccent()==0xffb166
            print("PASS: fresh process loads keyboard and controller action bindings.")
            raise SystemExit(0)
        for _ in range(4):press(0x40)
        press(0x4000)
        for _ in range(13):press(0x40)
        assert page()==1 and dll.MenuTestSnapshot()&15==13
        render("settings-keybinds.png");press(0x4000);assert page()==12
        render("keybinds.png");press(0x40);press(0x4000);assert page()==13
        render("binding-screenshot.png");press(0x4000)
        render("binding-key-recording.png");key(0x11,1);key(0x7b,1);key(0x7b,0);key(0x11,0)
        assert dll.MenuTestBinding(1,1)==0x17b
        press(0x40);press(0x4000)
        dll.MenuTestButton(0x802,1);pump(.4);dll.MenuTestButton(0x802,0);pump(.15)
        assert dll.MenuTestBinding(1,0)==0x802
        render("binding-screenshot-assigned.png")
        press(0x2000);press(0x2000);press(0x2000);assert page()==0
        render("main-action-keybind.png")
        dll.Ufc6ScreenshotCapability(1);key(0x11,1);key(0x7b,1)
        assert dll.Ufc6ScreenshotRequest()==1
        pump(.15);assert dll.Ufc6ScreenshotRequest()==0
        key(0x7b,0);key(0x11,0);dll.Ufc6ScreenshotResult(1)
        dll.MenuTestButtonDevice.argtypes=[c.c_int,w.DWORD,c.c_int]
        dll.MenuTestButtonDevice(1,2,1);dll.MenuTestButtonDevice(7,0x800,1);pump(.9)
        assert dll.Ufc6ScreenshotRequest()==0,"Combos must not combine different controllers"
        dll.MenuTestButtonDevice(1,2,0);dll.MenuTestButtonDevice(7,0x800,0);pump(.08)
        dll.MenuTestButton(0x802,1);pump(.9);assert dll.Ufc6ScreenshotRequest()==1
        pump(.15);assert dll.Ufc6ScreenshotRequest()==0
        dll.MenuTestButton(0x802,0);pump(.08);dll.Ufc6ScreenshotResult(1)
        dll.MenuTestInvokeBinding(6);pump(.15);assert page()==15 and dll.MenuTestSnapshot()&15==0
        render("quit-keybind-confirm.png");press(0x2000)
        for id,expected in [(5,2),(7,4),(8,3),(2,7),(3,1)]:
            dll.MenuTestInvokeBinding(id);pump(.15);assert page()==expected
            assert dll.MenuTestSnapshot()&0x100,"Hotkey must show a page, not execute exit/power"
        for _ in range(14):press(0x40)
        press(0x4000);assert page()==14
        render("themes.png");press(0x40);press(0x40);press(0x4000)
        render("theme-blue.png")
        press(0x10);press(0x10);press(0x4000)
        cfg=folder/"ufc6-overlay-theme.cfg"
        k.WritePrivateProfileStringW.argtypes=[w.LPCWSTR,w.LPCWSTR,w.LPCWSTR,w.LPCWSTR]
        original_cfg=cfg.read_bytes();cfg.write_text("[Theme]\nPreset=Purple\nAccent=#C060EE\n")
        pump(1.2);assert dll.MenuTestThemeAccent()==0xee60c0;render("theme-config-purple.png")
        if not args.no_render:assert (folder/"theme-blue.png").read_bytes()!=(folder/"theme-config-purple.png").read_bytes()
        cfg.write_bytes(original_cfg)
        press(0x40);press(0x40);press(0x4000);assert dll.MenuTestThemeChoice()==2
        print("PASS: in-frame keyboard/chord recording, saved bindings, once-per-press clean screenshot, shortcut badge, quit/restart/power confirmations, theme presets and config hot reload; no game or power actions executed.")
        raise SystemExit(0)
    elif args.longevity_save or args.longevity_reload or args.longevity_reload_off:
        expected=1 if args.longevity_reload else 0
        assert dll.MenuTestLongevity()==expected,"Saved longevity preference was not loaded in a fresh process"
        assert (dll.MenuTestBackendPreferences()&1)==expected,"Career initialization cleared the saved longevity choice"
        for _ in range(3):press(0x40)
        press(0x4000)
        for _ in range(7):press(0x40)
        if not args.longevity_reload_off:
            press(0x4000);assert dll.MenuTestLongevity()==1-expected
        k.GetPrivateProfileIntW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_int,w.LPCWSTR]
        assert k.GetPrivateProfileIntW("Menu","LongevityProtection",0,str(folder/"ufc6-quit-menu.test.ini"))==(0 if args.longevity_reload_off else 1-expected)
        print("PASS: longevity preference persists On/Off across fresh processes; no game memory changed.")
        raise SystemExit(0)
    elif args.training_save or args.training_reload or args.training_reload_off:
        assert dll.MenuTestBackendPreferences()>>8==(2 if args.training_reload else 1),"Career initialization cleared the saved multiplier"
        for _ in range(3):press(0x40)
        press(0x4000)
        for _ in range(8):press(0x40)
        assert dll.MenuTestSnapshot()&15==8
        press(0x4000)
        k.GetPrivateProfileIntW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_int,w.LPCWSTR]
        def saved():return k.GetPrivateProfileIntW("Menu","TrainingRewardMultiplier",0,str(folder/"ufc6-quit-menu.test.ini"))
        assert saved()==(5 if args.training_reload else 2),"Fresh process did not load the saved multiplier"
        if args.training_reload:
            press(0x80);press(0x80);assert saved()==1
        elif args.training_reload_off:
            press(0x80);assert saved()==1
        print("PASS: training reward multiplier and 1x Off persist across fresh processes.")
        raise SystemExit(0)
    elif args.game_icons:

        def page():return (dll.MenuTestSnapshot()>>12)&31
        render("main-unclipped.png")
        for _ in range(4):press(0x40)
        press(0x4000);render("settings-overlay-icons.png")
        for _ in range(10):press(0x40)
        render("settings-game-icons.png")
        press(0x4000);assert page()==9 and dll.MenuTestSnapshot()&15==2
        render("game-icons-ps.png")
        press(0x40);press(0x4000);assert page()==10 and dll.MenuTestSnapshot()&15==0
        render("game-icons-xbox-confirm.png")
        press(0x4000);assert page()==9 and dll.MenuTestGameIcons()==1
        press(0x40);press(0x4000);press(0x40);press(0x4000)
        assert page()==9 and dll.MenuTestGameIcons()==258
        render("game-icons-xbox.png")
        press(0x10);press(0x4000);assert page()==10
        render("game-icons-restore-confirm.png")
        press(0x40);press(0x4000);assert dll.MenuTestGameIcons()==513
        press(0x2000);assert page()==1 and dll.MenuTestSnapshot()&15==10
        print("PASS: distinct overlay/game icons, status, Cancel-first apply/restore, confirmation, Back; game files and launch mocked; all text rectangles fit their fonts.")
        raise SystemExit(0)
    elif args.cheats:
        dll.MenuTestCheatStatus.argtypes=[c.c_int,c.c_int]
        def cheat(i,value=-1):return dll.MenuTestCheatStatus(i,value)
        def page():return (dll.MenuTestSnapshot()>>12)&31
        render("main-menu-cheats.png")
        for _ in range(3):press(0x40)
        press(0x4000);assert page()==7
        assert cheat(0)==1 and cheat(1)==1
        render("cheats-player1.png")
        cheat(0,0);assert dll.MenuTestCheatEnabled(0)==0
        render("cheats-wait-off.png")
        press(0x4000);assert cheat(0)==0 and dll.MenuTestCheatEnabled(0)==1
        render("cheats-wait-on.png")
        press(0x4000);assert dll.MenuTestCheatEnabled(0)==0
        cheat(0,1)
        press(0x4000);assert cheat(0)==2 and cheat(1)==1
        press(0x40);press(0x20);assert cheat(0)==2 and cheat(1)==2
        render("cheats-on.png")
        if not args.no_render:
            assert (folder/"cheats-wait-off.png").read_bytes() != (folder/"cheats-wait-on.png").read_bytes(), "Switch change did not redraw the cached image"
        press(0x80);assert cheat(1)==1
        press(0x10);press(0x4000);assert cheat(0)==1
        cheat(0,7);press(0x4000);assert cheat(0)==1
        cheat(0,3);press(0x4000);assert cheat(0)==3
        render("cheats-launcher.png")
        cheat(0,4);press(0x4000);assert cheat(0)==4
        render("cheats-unavailable.png")
        cheat(0,1);press(0x4000);assert cheat(0)==2
        press(0x40);press(0x40);assert dll.MenuTestSnapshot()&15==2
        press(0x4000);assert dll.MenuTestKoState()==1;render("cheats-ko-armed.png")
        press(0x4000);assert dll.MenuTestKoState()==3;render("cheats-ko-idle.png")
        press(0x40);press(0x20);render("cheats-career-money.png")
        press(0x4000);assert page()==8 and dll.MenuTestSnapshot()&15==0
        render("career-add-money-confirm.png")
        press(0x4000);assert page()==7
        press(0x4000);press(0x40);press(0x4000);assert page()==7
        press(0x40);press(0x20);press(0x20);render("cheats-career-points.png")
        press(0x4000);assert page()==8
        render("career-add-points-confirm.png")
        press(0x2000);assert page()==7 and dll.MenuTestSnapshot()&15==4
        press(0x40);press(0x4000);assert page()==8
        render("career-fitness-confirm.png")
        press(0x40);press(0x4000);assert page()==7
        render("cheats-fitness.png")
        press(0x40);press(0x4000);assert page()==8
        render("career-training-days-confirm.png")
        press(0x40);press(0x4000);assert page()==7
        render("cheats-training-days.png")
        press(0x40);assert dll.MenuTestSnapshot()&15==7
        assert dll.MenuTestLongevity()==0
        press(0x4000);assert dll.MenuTestLongevity()==1 and page()==7
        render("cheats-longevity-on.png")
        press(0x80);assert dll.MenuTestLongevity()==0
        render("cheats-longevity-off.png")
        press(0x20);assert dll.MenuTestLongevity()==1
        press(0x40);press(0x20);render("cheats-training-rewards.png")
        press(0x40);press(0x4000);assert page()==8
        render("sparring-grade-a-confirm.png")
        press(0x40);press(0x4000);assert page()==7
        press(0x40);press(0x4000);assert page()==11
        render("savegame-backups.png")
        press(0x4000);assert page()==11
        render("savegame-backup-request.png")
        press(0x40);press(0x4000);assert page()==11
        press(0x2000);assert page()==7 and dll.MenuTestSnapshot()&15==10
        press(0x40);render("cheats-back-scrolled.png");press(0x4000);assert page()==0 and dll.MenuTestSnapshot()&15==3
        press(0x40);press(0x4000)
        for _ in range(9):press(0x40)
        press(0x4000);press(0x40);press(0x4000)
        assert page()==1 and cheat(0)==1 and dll.MenuTestLongevity()==0
        press(0x2000);press(0x2000);assert not dll.MenuTestSnapshot()&0x100
        print("PASS: main Cheats page, Player 1-only controls without a selector, confirm/left/right, Game On clears, launcher/unsupported gating, Back and reset; controller assignment tested by native fixture.")
        raise SystemExit(0)
    elif args.screenshot_reset:
        dll.Ufc6ScreenshotResultPath.argtypes=[w.BOOL,w.LPCWSTR]
        dll.MenuTestScreenshotName.argtypes=[w.LPWSTR,c.c_int]
        def page():return (dll.MenuTestSnapshot()>>12)&31
        def selected():return dll.MenuTestSnapshot()&15
        dll.Ufc6ScreenshotCapability(1)
        press(0x40);press(0x4000)
        assert dll.Ufc6ScreenshotRequest(),"Screenshot request missing"
        dll.Ufc6ScreenshotResultPath(True,r"C:\Pictures\Screenshots\UFC6_test.png");pump(.15)
        assert page()==5 and selected()==1,"Screenshot result must start on Back"
        name=c.create_unicode_buffer(128);dll.MenuTestScreenshotName(name,128)
        assert name.value=="UFC6_test.png"
        render("screenshot-saved.png")
        press(0x10);press(0x4000)
        assert dll.MenuTestFolderRequests()==1 and not dll.MenuTestSnapshot()&0x100
        dll.Ufc6ScreenshotResultPath(True,r"C:\Pictures\Screenshots\UFC6_hidden.png");pump(.1)
        assert not dll.MenuTestSnapshot()&0x100,"Late completion must not reopen overlay"
        dll.MenuTestToggle();pump(.1)
        for _ in range(4):press(0x40)
        press(0x4000);assert page()==1
        dll.Ufc6ScreenshotResultPath(True,r"C:\Pictures\Screenshots\UFC6_settings.png");pump(.1)
        assert page()==1,"Late completion must not replace Settings"
        press(0x4000) # PlayStation
        press(0x40);press(0x40);press(0x20) # scale 125
        press(0x40);press(0x4000) # auto launch Off
        press(0x40);press(0x4000) # Guide Off
        press(0x40);press(0x4000) # mocked Game Bar Off
        assert dll.MenuTestGameBar()==0
        ini=folder/"ufc6-quit-menu.test.ini"
        with ini.open("a",encoding="utf-8") as f:f.write("\n[Launch]\nMode=Custom\nExecutable=missing.exe\n[Other]\nKeep=yes\n")
        for _ in range(4):press(0x40)
        assert selected()==9
        render("settings-reset-row.png")
        press(0x4000);assert page()==6 and selected()==0
        render("reset-confirmation.png")
        press(0x4000);assert page()==1 and dll.MenuTestScale()==125,"Cancel changed settings"
        press(0x4000);press(0x40);press(0x4000)
        assert page()==1 and selected()==9
        assert dll.MenuTestScale()==100 and dll.MenuTestAutoLaunch()==1
        assert dll.MenuTestGuideEnabled()==1 and dll.MenuTestAccess()==0
        assert dll.MenuTestShortcut()==0xc08 and not dll.MenuTestSnapshot()&16
        assert dll.MenuTestGameBar()==0,"Reset changed Windows Game Bar preference"
        content=ini.read_text(encoding="utf-16")
        assert "[Launch]" not in content and "Keep=yes" in content
        assert "ButtonIcons=Xbox" in content and "GuideButtonOpensMenu=1" in content
        render("settings-reset-done.png")
        press(0x10);assert selected()==8
        press(0x4000)
        assert dll.MenuTestFolderRequests()==2 and not dll.MenuTestSnapshot()&0x100
        print("PASS: filename feedback, result-page defaults, folder actions, late capture focus, Cancel-first reset, persisted defaults, launcher reset and Game Bar preservation; Explorer mocked.")
        raise SystemExit(0)
    elif args.access or args.reload_access:
        dll.MenuTestPlayers.argtypes=[c.c_int,c.c_int];dll.MenuTestPlayers(42,7)
        dll.MenuTestButtonDevice.argtypes=[c.c_int,w.DWORD,c.c_int]
        def native(button,pad=42):
            dll.MenuTestButtonDevice(pad,button,1);dll.MenuTestButtonDevice(pad,button,0);pump(.06)
            if button in (0x10,0x40) and ((dll.MenuTestSnapshot()>>12)&31)==0 and dll.MenuTestSnapshot()&15==4:
                dll.MenuTestButtonDevice(pad,button,1);dll.MenuTestButtonDevice(pad,button,0);pump(.06)
        dll.MenuTestButtonDevice(7,0x20,0);pump(.04) # Player 2 is observed before Player 1.
        if args.reload_access:
            assert dll.MenuTestAccess()==1
            native(0x40,7);assert dll.MenuTestPlayer()==0 and dll.MenuTestSnapshot()&0xf==0
            native(0x40);assert dll.MenuTestPlayer()==1 and dll.MenuTestSnapshot()&0xf==1
            print("PASS: a fresh process loaded Player 1-only access and kept keyboard opening available.")
            raise SystemExit(0)
        assert dll.MenuTestAccess()==0
        native(0x40);assert dll.MenuTestPlayer()==1
        native(0x10);render("owner-player1.png");native(0x40)
        native(0x40,7);assert dll.MenuTestSnapshot()&0xf==1
        native(0x2000);dll.MenuTestToggle();pump(.1)
        native(0x40,7);assert dll.MenuTestPlayer()==2
        render("owner-player2.png")
        native(0x40,7);native(0x40,7);native(0x40,7);native(0x4000,7)
        for _ in range(7):native(0x40,7)
        assert dll.MenuTestSnapshot()&0xf==7
        render("access-either.png")
        dll.MenuTestButtonDevice(7,0x4000,1);pump(.25)
        assert dll.MenuTestAccess()==1 and dll.MenuTestPlayer()==0
        render("access-waiting-player1.png")
        dll.MenuTestButtonDevice(7,0x4000,0);pump(.05)
        native(0x40,7);assert dll.MenuTestSnapshot()&0xf==7 and dll.MenuTestPlayer()==0
        native(0x40);assert dll.MenuTestPlayer()==1 and dll.MenuTestSnapshot()&0xf==8
        render("access-player1-only.png")
        native(0x2000);native(0x2000)
        assert not dll.MenuTestSnapshot()&0x100
        dll.MenuTestButtonDevice(7,0xc08,1);pump(.95)
        assert not dll.MenuTestSnapshot()&0x100,"Player 2 chord must be blocked"
        dll.MenuTestButtonDevice(7,0xc08,0);pump(.06)
        dll.MenuTestButtonDevice(42,0xc08,1);pump(.95)
        dll.MenuTestButtonDevice(42,0xc08,0);pump(.06)
        assert dll.MenuTestSnapshot()&0x100 and dll.MenuTestPlayer()==1
        native(0x2000);dll.MenuTestToggle();pump(.08)
        native(0x40,7);assert dll.MenuTestPlayer()==0
        native(0x40);assert dll.MenuTestPlayer()==1
        assert "ControllerAccess=Player1" in (folder/"ufc6-quit-menu.test.ini").read_text()
        print("PASS: engine-assigned players with arbitrary IDs/arrival order, both owners, held-mode change, Player 2 rejection, Player 1 chord, keyboard access and persistence.")
        raise SystemExit(0)
    elif args.raw_access:
        dll.MenuTestRawPad.argtypes=[c.c_int,c.c_int,c.c_int]
        def raw(slot,buttons,connected=1):dll.MenuTestRawPad(slot,buttons,connected);pump(.05)
        def rpress(slot,buttons):
            raw(slot,buttons);raw(slot,0)
            if buttons in (1,2) and ((dll.MenuTestSnapshot()>>12)&31)==0 and dll.MenuTestSnapshot()&15==4:
                raw(slot,buttons);raw(slot,0)
        press(0x2000);raw(0,0);raw(1,0x400);raw(1,0)
        rpress(1,2);assert dll.MenuTestPlayer()==2
        rpress(0,2);assert dll.MenuTestSnapshot()&0xf==1
        for _ in range(3):rpress(1,2)
        rpress(1,0x1000)
        for _ in range(7):rpress(1,2)
        raw(1,0x1000);pump(.2)
        assert dll.MenuTestAccess()==1 and dll.MenuTestPlayer()==0
        raw(1,0);rpress(1,2);assert dll.MenuTestPlayer()==0
        rpress(0,2);assert dll.MenuTestPlayer()==1 and dll.MenuTestSnapshot()&0xf==8
        rpress(1,0x2000);assert (dll.MenuTestSnapshot()>>12)&31==1
        rpress(0,0x2000);rpress(0,0x2000)
        assert not dll.MenuTestSnapshot()&0x100
        raw(1,0x400);raw(1,0);assert not dll.MenuTestSnapshot()&0x100,"Player 2 Guide must be blocked"
        raw(0,0x400);raw(0,0);assert dll.MenuTestSnapshot()&0x100
        rpress(0,2);assert dll.MenuTestPlayer()==1
        raw(0,0,0);assert not dll.MenuTestSnapshot()&0x100
        raw(1,0x400);raw(1,0);assert not dll.MenuTestSnapshot()&0x100
        dll.MenuTestToggle();pump(.08);assert dll.MenuTestSnapshot()&0x100
        rpress(1,2);assert dll.MenuTestPlayer()==0
        dll.MenuTestToggle();pump(.08);assert not dll.MenuTestSnapshot()&0x100
        raw(0,0,1);dll.MenuTestToggle();pump(.08)
        for _ in range(4):rpress(0,2)
        rpress(0,0x1000)
        for _ in range(7):rpress(0,2)
        raw(1,0x400) # Held Guide becomes newly eligible when switching to Either.
        rpress(0,0x1000);pump(.15)
        assert dll.MenuTestAccess()==0 and dll.MenuTestSnapshot()&0x100,"A newly eligible held Guide must not close the menu"
        raw(1,0);raw(1,0x400);assert not dll.MenuTestSnapshot()&0x100
        raw(1,0)
        print("PASS: direct XInput owners, access transfer, Guide restriction/held-mode edge, disconnect and keyboard recovery.")
        raise SystemExit(0)
    elif args.raw_controllers:
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
        assert (dll.MenuTestSnapshot()>>12)&31==2
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
        for _ in range(5):
            dll.MenuTestButtonDevice(1,0x40,1);dll.MenuTestButtonDevice(1,0x40,0);pump(.05)
        dll.MenuTestButtonDevice(1,0x4000,1);dll.MenuTestButtonDevice(1,0x4000,0);pump(.07)
        assert (dll.MenuTestSnapshot()>>12)&31==1
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
        for _ in range(4):press(0x40)
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
        for _ in range(7):press(0x40)
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
        for _ in range(4):press(0x40)
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
        press(0x40);assert dll.MenuTestSnapshot()&0xf==8
        press(0x40);press(0x40);press(0x40);assert dll.MenuTestSnapshot()&0xf==11
        render("settings-updates.png")
        press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==1 and dll.MenuTestSnapshot()&0x100
        press(0x40);assert dll.MenuTestSnapshot()&0xf==12
        render("settings-diagnostics.png")
        press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==1 and dll.MenuTestSnapshot()&0x100
        press(0x40);press(0x40);press(0x40);assert dll.MenuTestSnapshot()&0xf==15
        render("settings-back.png")
        press(0x4000);assert (dll.MenuTestSnapshot()>>12)&31==0
        assert windows_preference()==original_windows,"UI test must never change the actual Windows Game Bar preference"
        print("PASS: Game Bar settings row, On/Off, confirm/left/right and Back; actual Windows preference unchanged.")
    elif args.guide or args.reload_guide_off:
        assert dll.MenuTestGuideEnabled()==(0 if args.reload_guide_off else 1)
        for _ in range(4):press(0x40)
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
            for _ in range(4):press(0x40)
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
            for _ in range(6):press(0x40)
        render("main-auto-off.png")
        press(0x4000)
        render("confirmation-auto-off.png")
        assert (dll.MenuTestSnapshot()>>12)&31==4
        print("PASS: auto-launch defaults On, toggles with confirm/left/right, saves Off, and renders status on both pages." if not args.reload_auto_off else "PASS: a fresh process loaded auto-launch Off and showed it in the reboot confirmation.")
    elif args.power or args.reboot:
        render("main-menu.png")
        for _ in range(6 if args.reboot else 7):press(0x40)
        press(0x4000)
        assert (dll.MenuTestSnapshot()>>12)&31 == (4 if args.reboot else 3) and dll.MenuTestSnapshot()&0xf == 0
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
        for _ in range(4):press(0x40)
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
        assert dll.MenuTestSnapshot()&(1<<20)
        render("record-shortcut.png")
        dll.MenuTestButton(2,1);dll.MenuTestButton(4,1);pump(.45)
        dll.MenuTestButton(2,0);dll.MenuTestButton(4,0);pump(.2)
        assert not dll.MenuTestSnapshot()&(1<<20)
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
