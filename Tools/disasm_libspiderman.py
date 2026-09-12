import sys,re,struct,subprocess
from elftools.elf.elffile import ELFFile
from capstone import *
SO='apk/lib/armeabi/libspiderman.so'
elf=ELFFile(open(SO,'rb')); data=open(SO,'rb').read()
segs=[(s['p_vaddr'],s['p_offset'],s['p_filesz']) for s in elf.iter_segments() if s['p_type']=='PT_LOAD']
def v2o(v):
    for va,off,sz in segs:
        if va<=v<va+sz: return off+(v-va)
    return None
syms={}
for line in subprocess.run(['nm','-S','--demangle',SO],capture_output=True,text=True).stdout.splitlines():
    m=re.match(r'^([0-9a-f]+) ([0-9a-f]+) ([TtWw]) (.*)$',line)
    if m: a=int(m.group(1),16); syms[a&~1]=(int(m.group(2),16),m.group(4),a&1)
def find(name):
    for a,(sz,n,t) in syms.items():
        if name in n: return a,sz,n,t
def cstr(v):
    o=v2o(v)
    if o is None: return None
    e=data.find(b'\0',o,o+200)
    s=data[o:e]
    return s.decode('ascii') if e>o and all(32<=c<127 for c in s) else None
mdT=Cs(CS_ARCH_ARM,CS_MODE_THUMB); mdA=Cs(CS_ARCH_ARM,CS_MODE_ARM)
def dis(name,maxlines=70):
    a,sz,n,t=find(name); o=v2o(a)
    print('===',n,'@%x size %d %s'%(a,sz,'thumb' if t else 'arm'))
    lines=0
    md=mdT if t else mdA
    for ins in md.disasm(data[o:o+sz],a):
        note=''
        if ins.mnemonic in('bl','blx'):
            if ins.op_str.startswith('#'):
                tt=int(ins.op_str.lstrip('#'),16)&~1
                note='-> '+(syms[tt][1] if tt in syms else '?')
            else:
                note='-> (indirect '+ins.op_str+')'
        elif ins.mnemonic=='ldr' and '[pc' in ins.op_str:
            m=re.search(r'#(0x[0-9a-f]+|\d+)',ins.op_str)
            imm=int(m.group(1),0) if m else 0
            lit=(((ins.address+4)&~3) if t else (ins.address+8))+imm
            lo=v2o(lit)
            if lo:
                val=struct.unpack_from('<I',data,lo)[0]
                s=cstr(val)
                if s: note='= "%s"'%s
                elif (val&~1) in syms: note='= &'+syms[val&~1][1]
        if note:
            print('  %x  %-6s %-28s %s'%(ins.address,ins.mnemonic,ins.op_str,note)); lines+=1
        if lines>=maxlines: break
for n in sys.argv[1:]: dis(n)
