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
    """Split a phoneme run into WORDS - whitespace-delimited units.

    A word like IH<250,24>N is one word ("in"): the timing rides on the first
    phoneme and the rest of the word follows inside the same unit. Splitting
    between IH<250,24> and N would break the word across two utterances, which
    is audible. So whitespace is the only word boundary.
    """
    return re.sub(r'\s+', ' ', body).strip().split()

def subdivide(word, budget):
    """Last resort for a word longer than one utterance can hold.

    Some song files carry no whitespace at all - the entire song is a single
    "word" of several hundred characters - so there is nothing to split on but
    the phoneme tokens themselves. Only used when a word will not fit.
    """
    toks = re.findall(r'[^\s<]*?<\d+(?:,\d+)?>|\S+?(?=[A-Z]*<|$)', word)
    toks = [t for t in toks if t]
    out, cur, n = [], [], 0
    for t in toks:
        if cur and n + len(t) > budget:
            out.append(''.join(cur)); cur, n = [], 0
        cur.append(t); n += len(t)
    if cur: out.append(''.join(cur))
    return out or [word]

def split_run(words, budget, window=0.55):
    """Group words into utterances.

    The budget is a hard ceiling. Within it, break after the word carrying the
    longest duration - a long note usually ends a sung phrase, so the seam falls
    where a singer would breathe. Words are never split; a word too long to fit
    is subdivided first, and only then as a last resort.
    """
    def dur(w):
        return max((int(x) for x in re.findall(r'<(\d+)', w)), default=0)

    # any word that cannot fit becomes several
    flat = []
    for w in words:
        flat.extend(subdivide(w, budget) if len(w) > budget else [w])

    out, i = [], 0
    while i < len(flat):
        n, j = 0, i
        while j < len(flat):
            k = len(flat[j]) + (1 if j > i else 0)
            if n + k > budget:
                break
            n += k; j += 1
        if j >= len(flat):
            out.append(flat[i:]); break
        if j == i:
            out.append([flat[i]]); i += 1; continue
        # keep at least one candidate: lo must stay below j
        lo = min(i + max(1, int((j - i) * window)), j - 1)
        best = max(range(lo, j), key=lambda k: (dur(flat[k]), k))
        out.append(flat[i:best + 1])
        i = best + 1
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
