"""Read-only x86 PE inspection. Never executes or modifies the target."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / "TouhouFrameTool/research/pylibs"))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument("exe", type=Path)
    p.add_argument("--address", type=lambda v: int(v, 0))
    p.add_argument("--size", type=lambda v: int(v, 0), default=0x200)
    p.add_argument("--xref", type=lambda v: int(v, 0))
    p.add_argument("--live-text", type=Path, help="Use a private copy of the normally running .text")
    a = p.parse_args()
    raw = a.exe.read_bytes()
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    def code(address, size):
        if a.live_text:
            return a.live_text.read_bytes()[address-0x401000:address-0x401000+size]
        return pe.get_data(address-base, size)
    if a.address is not None:
        for ins in decoder.disasm(code(a.address, a.size), a.address):
            print(f"{ins.address:08x}  {ins.bytes.hex(' '):30} {ins.mnemonic:8} {ins.op_str}")
    elif a.xref is not None:
        for section in pe.sections:
            if not section.Characteristics & 0x20000000:
                continue
            decoder.skipdata = True
            for ins in decoder.disasm(code(base+section.VirtualAddress, section.Misc_VirtualSize), base + section.VirtualAddress):
                if ins.id == 0:
                    continue
                if any((op.type == X86_OP_IMM and op.imm == a.xref) or
                       (op.type == X86_OP_MEM and op.mem.disp == a.xref)
                       for op in ins.operands):
                    print(f"{ins.address:08x} {ins.mnemonic:8} {ins.op_str}")
    else:
        print(json.dumps({
            "path": str(a.exe.resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
            "machine": hex(pe.FILE_HEADER.Machine), "base": hex(base),
            "entry": hex(base + pe.OPTIONAL_HEADER.AddressOfEntryPoint),
            "sections": [{"name": s.Name.rstrip(b'\0').decode(),
                          "va": hex(base+s.VirtualAddress),
                          "size": hex(s.Misc_VirtualSize)} for s in pe.sections],
            "imports": {e.dll.decode(): [i.name.decode() if i.name else i.ordinal
                         for i in e.imports] for e in pe.DIRECTORY_ENTRY_IMPORT},
        }, indent=2))


if __name__ == "__main__":
    main()
