"""Local TH18 research helpers. Read access is the default."""
import ctypes as C
from ctypes import wintypes as W
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'TouhouFrameTool/research/pylibs'))
K = C.WinDLL('kernel32', use_last_error=True)
U = C.WinDLL('user32', use_last_error=True)
K.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
K.OpenProcess.restype = W.HANDLE
K.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
K.ReadProcessMemory.restype = W.BOOL
K.CloseHandle.argtypes = [W.HANDLE]
U.GetForegroundWindow.restype = W.HWND
U.SetForegroundWindow.argtypes = [W.HWND]
U.ShowWindow.argtypes = [W.HWND, C.c_int]
U.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
U.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
U.ClientToScreen.argtypes = [W.HWND, C.POINTER(W.POINT)]
U.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
CALLBACK = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)


class Process:
    def __init__(self, pid, access=0x410):
        self.pid = pid
        self.handle = K.OpenProcess(access, False, pid)
        if not self.handle:
            raise C.WinError(C.get_last_error())

    def read(self, address, size):
        buf = C.create_string_buffer(size)
        n = C.c_size_t()
        if not K.ReadProcessMemory(self.handle, address, buf, size, C.byref(n)) or n.value != size:
            raise C.WinError(C.get_last_error())
        return buf.raw

    def u32(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def close(self):
        if self.handle:
            K.CloseHandle(self.handle)
            self.handle = None

    def __enter__(self): return self
    def __exit__(self, *_): self.close()


def window(pid):
    found = []
    def visit(hwnd, _):
        proc = W.DWORD()
        U.GetWindowThreadProcessId(hwnd, C.byref(proc))
        title = C.create_unicode_buffer(512)
        U.GetWindowTextW(hwnd, title, 512)
        if proc.value == pid and title.value:
            found.append((hwnd, title.value))
        return True
    U.EnumWindows(CALLBACK(visit), 0)
    if not found: raise RuntimeError('No game window for PID ' + str(pid))
    return found[0]


def capture(pid, output):
    from PIL import ImageGrab
    hwnd, title = window(pid)
    rect, origin = W.RECT(), W.POINT()
    U.GetClientRect(hwnd, C.byref(rect))
    U.ClientToScreen(hwnd, C.byref(origin))
    ImageGrab.grab(bbox=(origin.x, origin.y, origin.x+rect.right, origin.y+rect.bottom)).save(output)
    return title


if __name__ == '__main__':
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    pid = int(sys.argv[1])
    if sys.argv[2] == 'capture':
        print(capture(pid, sys.argv[3]))
    elif sys.argv[2] == 'dump-text':
        with Process(pid) as p:
            (ROOT/'research/live-text.bin').write_bytes(p.read(0x401000, 0xab7ca))
            print('Private local analysis copy of running .text saved.')
    else:
        addr = int(sys.argv[2], 0)
        size = int(sys.argv[3], 0) if len(sys.argv)>3 else 0x200
        with Process(pid) as p:
            for i in Cs(CS_ARCH_X86, CS_MODE_32).disasm(p.read(addr, size), addr):
                print(f'{i.address:08x} {i.mnemonic:8} {i.op_str}')
