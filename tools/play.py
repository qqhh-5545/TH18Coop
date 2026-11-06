"""Send bounded test input exclusively while the specified TH18 window is foreground."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import time
from pathlib import Path
import live

KEYS={'esc':0x1b,'enter':0x0d,'up':0x26,'down':0x28,'left':0x25,'right':0x27,
      'z':0x5a,'x':0x58,'c':0x43,'f7':0x76,'w':0x57,'a':0x41,'s':0x53,'d':0x44,
      'f':0x46,'g':0x47,'r':0x52,'h':0x48,'t':0x54,'shift':0x10,'ctrl':0x11,'alt':0x12}
def main():
    ap=argparse.ArgumentParser(__doc__)
    ap.add_argument('pid',type=int)
    ap.add_argument('--keys',default='')
    ap.add_argument('--hold',default='')
    ap.add_argument('--seconds',type=float,default=.8)
    ap.add_argument('--wait',type=float,default=.4)
    ap.add_argument('--key-gap',type=float,default=.5)
    ap.add_argument('--output',type=Path)
    ap.add_argument('--step-output',type=Path,help='Capture each menu step to this filename prefix')
    ap.add_argument('--pause-after',action='store_true',help='Pause this test run after capturing the frame')
    a=ap.parse_args()
    for name in filter(None,(a.keys+','+a.hold).split(',')):
        if name not in KEYS: ap.error('Unknown test key: '+name)
    if a.seconds<0 or a.seconds>10 or a.wait<0 or a.wait>10: ap.error('Wait and hold must be 0..10 seconds')
    if a.key_gap<0 or a.key_gap>3: ap.error('Key gap must be 0..3 seconds')
    hwnd,title=live.window(a.pid)
    if 'TH18' not in title: raise RuntimeError('Target is not a TH18 game window')
    live.U.ShowWindow(hwnd,9);live.U.SetForegroundWindow(hwnd);time.sleep(.4)
    # Request English input for this game window, so test shot keys do not enter an IME.
    live.U.LoadKeyboardLayoutW.argtypes=[W.LPCWSTR,W.UINT]
    live.U.LoadKeyboardLayoutW.restype=W.HANDLE
    live.U.SendMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
    layout=live.U.LoadKeyboardLayoutW('00000409',0)
    live.U.SendMessageW(hwnd,0x50,0,layout)
    live.U.SetForegroundWindow(hwnd);time.sleep(.3)
    def check():
        if live.U.GetForegroundWindow()!=hwnd: raise RuntimeError('Game lost focus; test input stopped')
    def key(name,up=False):
        check();vk=KEYS[name]
        scan=live.U.MapVirtualKeyW(vk,0)
        live.U.keybd_event(vk,scan,2 if up else 0,0)
    for step,name in enumerate(filter(None,a.keys.split(','))):
        key(name)
        try: time.sleep(.09)
        finally:
            # Always release a pressed key even if focus changes.
            vk=KEYS[name];live.U.keybd_event(vk,live.U.MapVirtualKeyW(vk,0),2,0)
        time.sleep(a.key_gap)
        if a.step_output: check();live.capture(a.pid,str(a.step_output)+f'-{step:02d}-{name}.png')
    held=[]
    try:
        for name in filter(None,a.hold.split(',')):
            key(name);held.append(name)
        if held:
            until=time.monotonic()+a.seconds
            while time.monotonic()<until: check();time.sleep(.02)
    finally:
        for name in held:
            vk=KEYS[name];live.U.keybd_event(vk,live.U.MapVirtualKeyW(vk,0),2,0)
    time.sleep(a.wait)
    if a.output: check();live.capture(a.pid,a.output)
    if a.pause_after:
        key('esc');time.sleep(.08);key('esc',True);time.sleep(.2)
    with live.Process(a.pid) as p:
        ptr=p.u32(0x4cf410)
        menu=p.u32(0x4cf43c)
        print({'player':hex(ptr),'mode':hex(p.u32(0x4cccc8)),'stage':p.u32(0x4cccdc),
               'state':p.u32(ptr+0x476ac) if ptr else None,
               'menu':(p.u32(menu+0x18),p.u32(menu+0x20)) if menu else None})

if __name__=='__main__': main()
