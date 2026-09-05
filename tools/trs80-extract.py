#!/usr/bin/env python3
"""
Read a TRS-80 JV3 floppy image: list its directory, and extract files,
detokenizing Level II / Model III BASIC on the way out.

    trs80-extract.py DISK.JV3 --list
    trs80-extract.py DISK.JV3 --cat SINGCOMP/TXT
    trs80-extract.py DISK.JV3 --cat SPEAKSNG/BAS --detok
    trs80-extract.py DISK.JV3 --raw > flat.img       # ordered sector dump

JV3 layout: 2901 three-byte sector headers (track, sector, flags), a write
protect byte, then the sector data in header order. Free headers have track
0xFF. Flag bits 0-1 give the size, bit 4 the side.

Extraction is by content scan over the ordered image rather than by walking
TRSDOS granule chains, which is enough for the contiguous files these demo
disks use. --list does parse the directory on track 17.
"""
import argparse, collections, re, sys

JV3_SIZES = {0: 256, 1: 128, 2: 1024, 3: 512}
SECTORS_PER_TRACK = 18
DIR_TRACK = 17

TOK={0x80:'END',0x81:'FOR',0x82:'RESET',0x83:'SET',0x84:'CLS',0x85:'CMD',0x86:'RANDOM',0x87:'NEXT',
0x88:'DATA',0x89:'INPUT',0x8A:'DIM',0x8B:'READ',0x8C:'LET',0x8D:'GOTO',0x8E:'RUN',0x8F:'IF',
0x90:'RESTORE',0x91:'GOSUB',0x92:'RETURN',0x93:'REM',0x94:'STOP',0x95:'ELSE',0x96:'TRON',0x97:'TROFF',
0x98:'DEFSTR',0x99:'DEFINT',0x9A:'DEFSNG',0x9B:'DEFDBL',0x9C:'LINE',0x9D:'EDIT',0x9E:'ERROR',0x9F:'RESUME',
0xA0:'OUT',0xA1:'ON',0xA2:'OPEN',0xA3:'FIELD',0xA4:'GET',0xA5:'PUT',0xA6:'CLOSE',0xA7:'LOAD',
0xA8:'MERGE',0xA9:'NAME',0xAA:'KILL',0xAB:'LSET',0xAC:'RSET',0xAD:'SAVE',0xAE:'SYSTEM',0xAF:'LPRINT',
0xB0:'DEF',0xB1:'POKE',0xB2:'PRINT',0xB3:'CONT',0xB4:'LIST',0xB5:'LLIST',0xB6:'DELETE',0xB7:'AUTO',
0xB8:'CLEAR',0xB9:'CLOAD',0xBA:'CSAVE',0xBB:'NEW',0xBC:'TAB(',0xBD:'TO',0xBE:'FN',0xBF:'USING',
0xC0:'VARPTR',0xC1:'USR',0xC2:'ERL',0xC3:'ERR',0xC4:'STRING$',0xC5:'INSTR',0xC6:'POINT',0xC7:'TIME$',
0xC8:'MEM',0xC9:'INKEY$',0xCA:'THEN',0xCB:'NOT',0xCC:'STEP',0xCD:'+',0xCE:'-',0xCF:'*',0xD0:'/',
0xD1:'[',0xD2:'AND',0xD3:'OR',0xD4:'>',0xD5:'=',0xD6:'<',0xD7:'SGN',0xD8:'INT',0xD9:'ABS',0xDA:'FRE',
0xDB:'INP',0xDC:'POS',0xDD:'SQR',0xDE:'RND',0xDF:'LOG',0xE0:'EXP',0xE1:'COS',0xE2:'SIN',0xE3:'TAN',
0xE4:'ATN',0xE5:'PEEK',0xE6:'CVI',0xE7:'CVS',0xE8:'CVD',0xE9:'EOF',0xEA:'LOC',0xEB:'LOF',0xEC:'MKI$',
0xED:'MKS$',0xEE:'MKD$',0xEF:'CINT',0xF0:'CSNG',0xF1:'CDBL',0xF2:'FIX',0xF3:'LEN',0xF4:'STR$',
0xF5:'VAL',0xF6:'ASC',0xF7:'CHR$',0xF8:'LEFT$',0xF9:'RIGHT$',0xFA:'MID$',0xFB:"'"}

def text_run(img, i, cap=16000):
    """Length of the printable-ASCII run at i, stopping at sector padding."""
    seg = img[i:i+cap]
    end = len(seg)
    m = re.search(rb'[ \x00]{80,}', seg)
    if m:
        end = m.start()
    for j, c in enumerate(seg[:end]):
        if not (32 <= c < 127 or c in (9, 10, 13)):
            return j
    return end

def read_jv3(path):
    d = open(path, 'rb').read()
    hdr, off, sectors = d[:8703], 8704, collections.OrderedDict()
    for i in range(2901):
        t, s, f = hdr[3*i], hdr[3*i+1], hdr[3*i+2]
        if t == 0xFF:
            continue
        size = JV3_SIZES[f & 3]
        sectors[(t, 1 if f & 0x10 else 0, s)] = d[off:off+size]
        off += size
    return sectors

def flatten(sectors):
    return b''.join(sectors[k] for k in sorted(sectors))

def directory(img):
    """TRSDOS entries live on track 17, 32 bytes each: name at +5, ext at +13."""
    out = []
    for s in range(2, 10):
        base = (DIR_TRACK * SECTORS_PER_TRACK + s) * 256
        blk = img[base:base+256]
        for e in range(0, 256, 32):
            ent = blk[e:e+32]
            if ent[:1] in (b'\x00', b'\xff'):
                continue
            if not all(32 <= c < 127 for c in ent[5:16]):
                continue
            name = ent[5:13].decode('latin-1').strip()
            ext = ent[13:16].decode('latin-1').strip()
            if name:
                out.append(f"{name}/{ext}")
    return out

def detok(d, pos):
    out=[]
    if d[pos]==0xFF: pos+=1
    while pos+4 <= len(d):
        nxt=d[pos]|(d[pos+1]<<8)
        if nxt==0: break
        line=d[pos+2]|(d[pos+3]<<8)
        pos+=4
        s=[]; instr=False
        while pos<len(d) and d[pos]!=0:
            c=d[pos]
            if c==0x22: instr=not instr
            if not instr and c in TOK: s.append(TOK[c])
            else: s.append(chr(c))
            pos+=1
        pos+=1
        out.append(f"{line} {''.join(s)}")
        if line>65000: break
    return out

def detokenize(img, start):
    return '\n'.join(detok(img, start))

def find_basic_start(img, near):
    """Tokenized BASIC on disk starts with 0xFF, then 2-byte link, 2-byte line."""
    for p in range(near, max(0, near - 8000), -1):
        if img[p] == 0xFF and img[p+1] != 0xFF and (img[p+3] | (img[p+4] << 8)) < 100:
            return p
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('image')
    ap.add_argument('--list', action='store_true', help='list the TRSDOS directory')
    ap.add_argument('--cat', metavar='NAME', help='extract a file by a string it contains, or its name')
    ap.add_argument('--detok', action='store_true', help='detokenize as Level II BASIC')
    ap.add_argument('--raw', action='store_true', help='dump the ordered sector image')
    a = ap.parse_args()

    img = flatten(read_jv3(a.image))
    if a.raw:
        sys.stdout.buffer.write(img); return
    if a.list:
        for n in directory(img):
            print(n)
        return
    if not a.cat:
        ap.error('give --list, --cat or --raw')

    needle = a.cat.encode('latin-1')
    hits = [m.start() for m in re.finditer(re.escape(needle), img)]
    if not hits:
        sys.exit(f"{a.cat!r} not found in {a.image}")
    # The same string often appears both inside a tokenized BASIC program and in
    # the standalone data file. For a text extract, take whichever occurrence
    # yields the longest clean run - that is the real file.
    i = hits[0] if a.detok else max(hits, key=lambda h: text_run(img, h))
    if a.detok:
        start = find_basic_start(img, i)
        if start is None:
            sys.exit('no tokenized BASIC header found before that offset')
        print(detokenize(img, start))
    else:
        # A TRSDOS text file is plain ASCII padded to a sector boundary. Stop at
        # the padding, or at the first byte that is not text - otherwise a hit
        # inside a tokenized BASIC program runs on into the rest of the disk.
        end = text_run(img, i)
        seg = img[i:i+end]
        if end == 0:
            sys.exit(f"{a.cat!r} matched inside binary - if it is a BASIC "
                     f"program, pass --detok")
        sys.stdout.write(seg.decode('latin-1').replace('\r', '\n'))

if __name__ == '__main__':
    main()
