"""Check hook instruction boundaries against a private local running-image dump.

The dump is not distributed. This tool neither launches nor attaches to a game.
Requires Capstone; other tools use the same optional analysis dependency.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent / 'TouhouFrameTool/research/pylibs'))
from capstone import Cs,CS_ARCH_X86,CS_MODE_32,CS_GRP_JUMP,CS_GRP_CALL,CS_GRP_RET

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('dump',type=Path,help='Private .text copy beginning at 0x401000')
    args=parser.parse_args()
    data=args.dump.read_bytes();source=(ROOT/'src/coop.c').read_text(encoding='utf-8')
    arrays={}
    for match in re.finditer(r'const unsigned char (\w+)\[\]\s*=\{([^}]+)\}',source):
        arrays[match[1]]=bytes(int(value.strip(),0) for value in match[2].split(',') if value.strip())
    decoder=Cs(CS_ARCH_X86,CS_MODE_32);decoder.detail=True;count=0
    for match in re.finditer(r'trampoline\((0x[\da-fA-F]+),(\d+),(\w+)\)',source):
        address=int(match[1],16);length=int(match[2]);expected=arrays[match[3]]
        assert len(expected)==length,(hex(address),'signature length')
        assert data[address-0x401000:address-0x401000+length]==expected,(hex(address),'profile differs')
        instructions=list(decoder.disasm(expected,address))
        assert sum(ins.size for ins in instructions)==length,(hex(address),'split instruction')
        assert not any(any(ins.group(group) for group in (CS_GRP_JUMP,CS_GRP_CALL,CS_GRP_RET)) for ins in instructions),(hex(address),'nonrelocatable prologue')
        count+=1
    assert count>=17,'Missing hook coverage'
    print(f'PASS {count} exact native signatures; complete relocatable x86 prologues')
    # Root scheduler consumers, not just the callee: 0 is application quit,
    # -1 is error, positive results continue. Teardown dispatches with +0x54.
    callers={
        0x471b53:'e888f7f8ff85c00f84e103000083f8ff0f84c2030000',
        0x472e97:'e844e4f8ff85c07511b984d84c00e886fcf8ffb8010000005ec383f8ff7511b984d84c00e870fcf8ffb8020000005ec3',
        0x472ff0:'e8ebe2f8ff85c07511b984d84c00e82dfbf8ffb8010000005ec383f8ff7511b984d84c00e817fbf8ffb8020000005ec3',
        0x471fb6:'8bcec7465401000000e81cf3f8ff',
    }
    for address,signature in callers.items():
        expected=bytes.fromhex(signature)
        assert data[address-0x401000:address-0x401000+len(expected)]==expected,(hex(address),'scheduler caller contract changed')
    print('PASS native scheduler callers: zero quits, minus one errors, shutdown dispatch uses +0x54')
    loading={
        # Native worker start: flags, beginthreadex and stored thread handle.
        0x454887:'8b4508688cd84c006a006a00506a006a00a39cd84c00c70594d84c0001000000c70590d84c0000000000e8b8e5030083c418a388d84c00',
        # Scene worker publishes flags before its epilogue; wait for handle too.
        0x443218:'c70594d84c0000000000c70590d84c0001000000',
        # Worker waits on audio queue serviced by the main GameThread.
        0x443153:'833d04d25600008b35acd04a00740d6a10ffd6833d04d256000075f3',
        0x453494:'b980ad5600e882380200',
        # Return-to-title has a separate menu-owned job, not 4cd888.
        0x464cd9:'8db77c5d00008bcee84adef9ff8d4e08c74618804b4600516a005768804b46006a006a00c7461001000000c7460c00000000e85ee1020083c4188946048bc75f',
        # Menu worker returns without clearing its +5d8c control flag.
        0x464c5e:'a13cf44c005f5e8b40088348040233c0c70500ad560001000000c2',
    }
    for address,signature in loading.items():
        expected=bytes.fromhex(signature)
        assert data[address-0x401000:address-0x401000+len(expected)]==expected,(hex(address),'native scene loader contract changed')
    print('PASS native scene and independent menu workers: thread handles, control flags and audio service')
    rules={
        # Boss-only total damage hook and ECL boss registration layout.
        0x42f2b9:'f6833051000010',
        0x42d497:'8b15d0f24c008b450885c974108b8930680000894c8248',
        # Native item pickup radius; no hard-coded substitute in allocation.
        0x44624a:'f30f10868c790400f30f59c00f2fc10f867b030000',
        # Initial Bomb base and per-life reset use the same native word.
        0x442785:'c70560cd4c0003000000',
        0x45d1cb:'a160cd4c00',
        # Native active-card HUD uses the second recharge timer.
        0x4088bf:'660f6e4348f30f104b3c',
        # Native active HUD: script zero, original transform/layer, register.
        0x407f5c:'ff8334010000e8b91308008bf08bcb6a0056e8adf4ffff',
        0x407f9f:'c7464400000000e8d5050700568d442414c786200500000000000050e890030800',
        # A resident pause object is active only when its +1ec is nonzero.
        0x418993:'a10cf44c0083ec14578bf985c0740d83b8ec010000000f8565020000',
    }
    for address,signature in rules.items():
        expected=bytes.fromhex(signature)
        assert data[address-0x401000:address-0x401000+len(expected)]==expected,(hex(address),'cooperative rule contract changed')
        assert sum(ins.size for ins in decoder.disasm(expected,address))==len(expected),(hex(address),'split rule instruction')
    assert arrays['boss_damage_start']==bytes.fromhex(rules[0x42f2b9]),'boss bridge signature drift'
    print('PASS Boss/pickup/Bomb/timer contracts; original active HUD root construction and resident pause state')

if __name__=='__main__': main()
