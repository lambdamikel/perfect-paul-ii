#!/usr/bin/env python3
"""
Turn a DECtalk song or script into a paced Applesoft listing for Perfect Paul ][.

    dt2applesoft.py SONG.TXT --title "AWAY IN A MANGER" > AWAY.bas

Then import it with AppleCommander:

    java -jar AppleCommander-ac.jar -bas disk.dsk AWAY < AWAY.bas

Why the pacing exists: the card is write-only, with no status register and a
line queue eight utterances deep. When that queue fills, the firmware blocks
core 0, stops draining the bus FIFO, and drops characters silently - you hear
speech garble rather than an error. So the sender has to pace itself, and each
utterance carries its own length in milliseconds, taken from the <duration>
fields, which makes the pacing derived rather than guessed.
"""
import argparse, re, sys

MODE = "[:PHONE ARPA SPEAK ON]"          # the spelling DAISY proved on hardware
VOICES = {'np','nb','nh','nf','nd','nk','nu','nr','nw'}   # DECtalkMini has no [:nv]
MAX_UTT = 254                             # firmware truncates beyond this
MAX_LINE = 239                            # Applesoft input line limit

def tokenize(body):
    """Split a phoneme run into whole tokens: NAME<dur[,pitch]>, or bare text."""
    return re.findall(r'[^\s<]*?<\d+(?:,\d+)?>|\S+', body)

def split_run(tokens, budget):
    """Group tokens into utterances. Prefer to break before a token carrying an
    explicit pitch - the start of a note, never mid-syllable - but the budget is
    a hard limit, because a long stretch may contain no pitched token at all."""
    out, cur, n = [], [], 0
    for t in tokens:
        w = len(t) + (1 if cur else 0)
        starts_note = bool(re.search(r'<\d+,\d+>', t))
        if cur and (n + w > budget or (starts_note and n > budget * 0.75)):
            out.append(cur); cur, n, w = [], 0, len(t)
        cur.append(t); n += w
    if cur: out.append(cur)
    return out

def ms_of(s):
    return sum(int(x) for x in re.findall(r'<(\d+)', s))

def convert(text, voice_sub='nh', keep_dv=False):
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    # what voice does the file ask for?
    m = re.search(r'\[:(n[a-z])\]', text, re.I)
    voice = (m.group(1).lower() if m else 'np')
    substituted = None
    if voice not in VOICES:
        substituted, voice = voice, voice_sub
    dv = re.findall(r'\[:dv[^\]]*\]', text, re.I) if keep_dv else []
    # every bracketed phoneme group (not a [: command)
    groups = re.findall(r'\[(?!:)([^\]]*)\]', text, re.S)
    # two ceilings: the firmware truncates an utterance past MAX_UTT, and the
    # DATA line that carries it must fit Applesoft's MAX_LINE with room for the
    # line number, DATA, the quotes and the millisecond field.
    budget = min(MAX_UTT - len(MODE) - 8, MAX_LINE - 24)
    utts = []
    for g in groups:
        for chunk in split_run(tokenize(re.sub(r'\s+', ' ', g).strip()), budget):
            s = ' '.join(chunk).upper()
            utts.append((f"[{s}]", ms_of(s)))
    return utts, voice, substituted, dv

def emit(utts, voice, title, source, substituted, dv):
    L, n = [], 10
    def add(t):
        nonlocal n
        L.append(f"{n}{'  ' if t.startswith('REM') else ' '}{t}"); n += 10
    add(f'REM PERFECT PAUL ][ SINGS'); add(f'REM {title}')
    for line in source: add(f'REM {line}')
    if substituted:
        add(f'REM SOURCE ASKED FOR [:{substituted.upper()}], WHICH')
        add(f'REM THIS DECTALK LACKS. USING [:{voice.upper()}].')
    add('REM ---- PACING: MS PER UTTERANCE, LESS')
    add('REM      THE TIME SPENT SENDING IT.')
    add('REM      LOWER PF TO TIGHTEN.')
    add('PF = .9: REM PACING FACTOR')
    add('SD = 3: REM LOOPS PER CHAR TO SEND')
    add('DR =  - 16192: REM SLOT 4  ($C0C0)')
    add('HOME ')
    add('PRINT "PERFECT PAUL ][": PRINT ')
    add(f'PRINT "{title}": PRINT ')
    add(f'M$ = "{MODE}[:{voice.upper()}]"')
    for d in dv: add(f'S$ = "{d.upper()}":MS = 400: GOSUB 900')
    add(f'FOR L = 1 TO {len(utts)}')
    add('READ P$,MS')
    add(f'PRINT "  PHRASE ";L;" OF {len(utts)}"')
    add('S$ = M$ + P$: GOSUB 900')
    add('NEXT L')
    add('PRINT : PRINT "DONE."')
    add('END ')
    L += ['900  REM ---- SEND, THEN WAIT OUT THE MS',
          '910  FOR I = 1 TO  LEN (S$): POKE DR, ASC ( MID$ (S$,I,1)): NEXT I',
          '920  POKE DR,13',
          '930 WT = MS * .9 * PF -  LEN (S$) * SD',
          '940  IF WT > 0 THEN  FOR W = 1 TO WT: NEXT W',
          '950  RETURN ']
    for i, (p, ms) in enumerate(utts):
        L.append(f'{2000 + 10*i}  DATA "{p}",{ms}')
    return L

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('infile'); ap.add_argument('--title', required=True)
    ap.add_argument('--credit', action='append', default=[],
                    help='REM line for provenance; repeatable')
    ap.add_argument('--voice-sub', default='nh')
    ap.add_argument('--keep-dv', action='store_true',
                    help='keep [:dv] voice shaping (support is unverified)')
    a = ap.parse_args()
    text = open(a.infile, 'rb').read().decode('latin-1')
    utts, voice, sub, dv = convert(text, a.voice_sub, a.keep_dv)
    lines = emit(utts, voice, a.title.upper(), [c.upper() for c in a.credit], sub, dv)

    bad = [l for l in lines if len(l) > MAX_LINE]
    rem = [l for l in lines if re.match(r'^\d+\s+REM\s*$', l)]   # these eat the next line
    long = [p for p, _ in utts if len(p) + len(MODE) + 8 > MAX_UTT]
    for label, bad_set in (('lines over 239 chars', bad),
                           ('bare REM lines', rem),
                           ('utterances over 254 chars', long)):
        if bad_set:
            sys.exit(f"refusing to emit: {len(bad_set)} {label}")
    print('\n'.join(lines))
    print(f"# {len(utts)} utterances, {sum(m for _,m in utts)/1000:.1f}s of music",
          file=sys.stderr)

if __name__ == '__main__':
    main()
