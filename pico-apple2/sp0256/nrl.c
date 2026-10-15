/* English text to SP0256-AL2 allophones: the NRL letter-to-sound rules.
   SPDX-License-Identifier: GPL-3.0-or-later
   See nrl.h for provenance. */

#include "nrl.h"
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ stage 1
   The rule notation, straight from the report. A rule is

       left context [ letters ] right context  =>  phonemes

   and the context classes are single characters:

       (space)  a word boundary: anything that is not a letter
       #        one or more vowels
       :        zero or more consonants
       ^        one consonant
       .        one voiced consonant          B D V G J L M N R W Z
       +        one front vowel               E I Y
       %        a suffix                      E ER ES ED ING ELY
       &        one sibilant                  S C G Z X J, CH, SH
       @        a consonant after which long U is /u/ rather than /ju/
                                              T S R D L Z N J, TH, CH, SH

   Rules are tried in order within the group selected by the first letter of
   the bracketed part, so the longest and most specific come first and the
   bare letter is always last. Phonemes are written with vowels in upper case
   and consonants in lower, which is what keeps them unambiguous in one
   string. */

typedef struct {
    const char *left;
    const char *match;
    const char *right;
    const char *out;
} nrl_rule_t;

static int is_letter(char c)   { return (c >= 'A' && c <= 'Z'); }
static int is_vowel(char c)    { return c=='A'||c=='E'||c=='I'||c=='O'||c=='U'; }
static int is_consonant(char c){ return is_letter(c) && !is_vowel(c); }
static int is_voiced(char c)   { return c=='B'||c=='D'||c=='V'||c=='G'||c=='J'||
                                       c=='L'||c=='M'||c=='N'||c=='R'||c=='W'||c=='Z'; }
static int is_front(char c)    { return c=='E'||c=='I'||c=='Y'; }

/* Sibilants and the long-U consonants include two-letter cases (CH, SH, TH),
   so they are tested against the text rather than one character. */
static int sibilant_at(const char *s, int i)
{
    const char c = s[i];
    if (c=='S'||c=='C'||c=='G'||c=='Z'||c=='X'||c=='J') return 1;
    if ((c=='C'||c=='S') && s[i+1]=='H') return 2;
    return 0;
}
static int longu_at(const char *s, int i)
{
    const char c = s[i];
    if (c=='T'||c=='S'||c=='R'||c=='D'||c=='L'||c=='Z'||c=='N'||c=='J') return 1;
    if ((c=='T'||c=='C'||c=='S') && s[i+1]=='H') return 2;
    return 0;
}

/* A suffix: E, ER, ES, ED, ING or ELY, at s+i. */
static int suffix_at(const char *s, int i)
{
    if (!strncmp(s+i, "ING", 3)) return 3;
    if (!strncmp(s+i, "ELY", 3)) return 3;
    if (!strncmp(s+i, "ER", 2))  return 2;
    if (!strncmp(s+i, "ES", 2))  return 2;
    if (!strncmp(s+i, "ED", 2))  return 2;
    if (s[i] == 'E')             return 1;
    return 0;
}

/* Right context: walk the pattern forwards from position i. */
static int match_right(const char *s, int i, const char *p)
{
    for (; *p; ++p) {
        int n;
        switch (*p) {
        case ' ':
            if (is_letter(s[i])) return 0;
            break;                      /* a boundary consumes nothing */
        case '#':
            if (!is_vowel(s[i])) return 0;
            while (is_vowel(s[i])) ++i;
            continue;
        case ':':
            while (is_consonant(s[i])) ++i;
            continue;
        case '^':
            if (!is_consonant(s[i])) return 0;
            ++i; continue;
        case '.':
            if (!is_voiced(s[i])) return 0;
            ++i; continue;
        case '+':
            if (!is_front(s[i])) return 0;
            ++i; continue;
        case '%':
            n = suffix_at(s, i);
            if (!n) return 0;
            i += n; continue;
        case '&':
            n = sibilant_at(s, i);
            if (!n) return 0;
            i += n; continue;
        case '@':
            n = longu_at(s, i);
            if (!n) return 0;
            i += n; continue;
        default:
            if (s[i] != *p) return 0;
            ++i; continue;
        }
    }
    return 1;
}

/* Left context: walk the pattern BACKWARDS from the character before i, which
   is why the patterns read naturally left to right in the table. */
static int match_left(const char *s, int i, const char *p)
{
    int k = (int)strlen(p) - 1;
    --i;
    for (; k >= 0; --k) {
        const char c = p[k];
        switch (c) {
        case ' ':
            if (i >= 0 && is_letter(s[i])) return 0;
            break;
        case '#':
            if (i < 0 || !is_vowel(s[i])) return 0;
            while (i >= 0 && is_vowel(s[i])) --i;
            continue;
        case ':':
            while (i >= 0 && is_consonant(s[i])) --i;
            continue;
        case '^':
            if (i < 0 || !is_consonant(s[i])) return 0;
            --i; continue;
        case '.':
            if (i < 0 || !is_voiced(s[i])) return 0;
            --i; continue;
        case '+':
            if (i < 0 || !is_front(s[i])) return 0;
            --i; continue;
        case '&':
            if (i < 0) return 0;
            if (i >= 1 && s[i]=='H' && (s[i-1]=='C' || s[i-1]=='S')) { i -= 2; continue; }
            if (!sibilant_at(s, i)) return 0;
            --i; continue;
        case '@':
            if (i < 0) return 0;
            if (i >= 1 && s[i]=='H' && (s[i-1]=='T'||s[i-1]=='C'||s[i-1]=='S')) { i -= 2; continue; }
            if (!longu_at(s, i)) return 0;
            --i; continue;
        default:
            if (i < 0 || s[i] != c) return 0;
            --i; continue;
        }
    }
    return 1;
}

static const nrl_rule_t rules_punct[] = {
    {"",     " ",   "",      " "},
    {"",     "-",   "",      ""},
    {".",    "'S",  "",      "z"},
    {"#:.E", "'S",  "",      "z"},
    {"#",    "'S",  "",      "z"},
    {"",     "'",   "",      ""},
    {"",     ",",   "",      " "},
    {"",     ".",   "",      " "},
    {"",     "?",   "",      " "},
    {"",     "!",   "",      " "},
    {"",     ";",   "",      " "},
    {"",     ":",   "",      " "},
    {"",     NULL,  "",      NULL}
};

static const nrl_rule_t rules_a[] = {
    {" ",  "A",     " ",     "AX"},
    {" ",  "ARE",   " ",     "AA r"},
    {" ",  "AR",    "O",     "AX r"},
    {"",   "AR",    "#",     "EH r"},
    {"^",  "AS",    "#",     "EY s"},
    {"",   "A",     "WA",    "AX"},
    {"",   "AW",    "",      "AO"},
    {" :", "ANY",   "",      "EH n IY"},
    {"",   "A",     "^+#",   "EY"},
    {"#:", "ALLY",  "",      "AX l IY"},
    {" ",  "AL",    "#",     "AX l"},
    {"",   "AGAIN", "",      "AX g EH n"},
    {"#:", "AG",    "E",     "IH j"},
    {"",   "A",     "^+:#",  "AE"},
    {" :", "A",     "^+ ",   "EY"},
    {"",   "A",     "^%",    "EY"},
    {" ",  "ARR",   "",      "AX r"},
    {"",   "ARR",   "",      "AE r"},
    {" :", "AR",    " ",     "AA r"},
    {"",   "AR",    " ",     "ER"},
    {"",   "AR",    "",      "AA r"},
    {"",   "AIR",   "",      "EH r"},
    {"",   "AI",    "",      "EY"},
    {"",   "AY",    "",      "EY"},
    {"",   "AU",    "",      "AO"},
    {"#:", "ALS",   " ",     "AX l z"},
    {"#:", "AL",    " ",     "AX l"},
    {"",   "ALK",   "",      "AO k"},
    {"",   "A",     "L^",    "AO"},
    {" :", "ABLE",  "",      "EY b AX l"},
    {"",   "ABLE",  "",      "AX b AX l"},
    {"",   "ANG",   "+",     "EY n j"},
    {"",   "A",     "",      "AE"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_b[] = {
    {" ",  "BE",    "^#",    "b IH"},
    {"",   "BEING", "",      "b IY IH NG"},
    {" ",  "BOTH",  " ",     "b OW TH"},
    {" ",  "BUS",   "#",     "b IH z"},
    {"",   "BUIL",  "",      "b IH l"},
    {"",   "B",     "",      "b"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_c[] = {
    {" ",  "CH",    "^",     "k"},
    {"^E", "CH",    "",      "k"},
    {"",   "CH",    "",      "CH"},
    {" S", "CI",    "#",     "s AY"},
    {"",   "CI",    "A",     "SH"},
    {"",   "CI",    "O",     "SH"},
    {"",   "CI",    "EN",    "SH"},
    {"",   "C",     "+",     "s"},
    {"",   "CK",    "",      "k"},
    {"",   "COM",   "%",     "k AH m"},
    {"",   "C",     "",      "k"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_d[] = {
    {"#:",   "DED", " ",     "d IH d"},
    {".E",   "D",   " ",     "d"},
    {"#:^E", "D",   " ",     "t"},
    {" ",    "DE",  "^#",    "d IH"},
    {" ",    "DO",  " ",     "d UW"},
    {" ",    "DOES","",      "d AH z"},
    {" ",    "DOING","",     "d UW IH NG"},
    {" ",    "DOW", "",      "d AW"},
    {"",     "DU",  "A",     "j UW"},
    {"",     "D",   "",      "d"},
    {"",     NULL,  "",      NULL}
};

static const nrl_rule_t rules_e[] = {
    {"#:",   "E",    " ",    ""},
    {"':^",  "E",    " ",    ""},
    {" :",   "E",    " ",    "IY"},
    {"#",    "ED",   " ",    "d"},
    {"#:",   "E",    "D ",   ""},
    {"",     "EV",   "ER",   "EH v"},
    {"",     "E",    "^%",   "IY"},
    {"",     "ERI",  "#",    "IY r IY"},
    {"",     "ERI",  "",     "EH r IH"},
    {"#:",   "ER",   "#",    "ER"},
    {"",     "ER",   "#",    "EH r"},
    {"",     "ER",   "",     "ER"},
    {" ",    "EVEN", "",     "IY v EH n"},
    {"#:",   "E",    "W",    ""},
    {"@",    "EW",   "",     "UW"},
    {"",     "EW",   "",     "y UW"},
    {"",     "E",    "O",    "IY"},
    {"#:&",  "ES",   " ",    "IH z"},
    {"#:",   "E",    "S ",   ""},
    {"#:",   "ELY",  " ",    "l IY"},
    {"#:",   "EMENT","",     "m EH n t"},
    {"",     "EFUL", "",     "f UH l"},
    {"",     "EE",   "",     "IY"},
    {"",     "EARN", "",     "ER n"},
    {" ",    "EAR",  "^",    "ER"},
    {"",     "EAD",  "",     "EH d"},
    {"#:",   "EA",   " ",    "IY AX"},
    {"",     "EA",   "SU",   "EH"},
    {"",     "EA",   "",     "IY"},
    {"",     "EIGH", "",     "EY"},
    {"",     "EI",   "",     "IY"},
    {" ",    "EYE",  "",     "AY"},
    {"",     "EY",   "",     "IY"},
    {"",     "EU",   "",     "y UW"},
    {"",     "E",    "",     "EH"},
    {"",     NULL,   "",     NULL}
};

static const nrl_rule_t rules_f[] = {
    {"",   "FUL",   "",      "f UH l"},
    {"",   "F",     "",      "f"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_g[] = {
    {"",   "GIV",   "",      "g IH v"},
    {" ",  "G",     "I^",    "g"},
    {"",   "GE",    "T",     "g EH"},
    {"SU", "GGES",  "",      "g j EH s"},
    {"",   "GG",    "",      "g"},
    {" B#","G",     "",      "g"},
    {"",   "G",     "+",     "j"},
    {"",   "GREAT", "",      "g r EY t"},
    {"#",  "GH",    "",      ""},
    {"",   "G",     "",      "g"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_h[] = {
    {" ",  "HAV",   "",      "h AE v"},
    {" ",  "HERE",  "",      "h IY r"},
    {" ",  "HOUR",  "",      "AW ER"},
    {"",   "HOW",   "",      "h AW"},
    {"",   "H",     "#",     "h"},
    {"",   "H",     "",      ""},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_i[] = {
    {" ",    "IN",   "",     "IH n"},
    {" ",    "I",    " ",    "AY"},
    {"",     "IN",   "D",    "AY n"},
    {"",     "IER",  "",     "IY ER"},
    {"#:R",  "IED",  " ",    "IY d"},
    {"",     "IED",  " ",    "AY d"},
    {"",     "IEN",  "",     "IY EH n"},
    {"",     "IE",   "T",    "AY EH"},
    {" :",   "I",    "%",    "AY"},
    {"",     "I",    "%",    "IY"},
    {"",     "IE",   "",     "IY"},
    {"",     "I",    "^+:#", "IH"},
    {"",     "IR",   "#",    "AY r"},
    {"",     "IZ",   "%",    "AY z"},
    {"",     "IS",   "%",    "AY z"},
    {"",     "I",    "D%",   "AY"},
    {"+^",   "I",    "^+",   "IH"},
    {"",     "I",    "T%",   "AY"},
    {"#:^",  "I",    "^+",   "IH"},
    {"",     "I",    "^+",   "AY"},
    {"",     "IR",   "",     "ER"},
    {"",     "IGH",  "",     "AY"},
    {"",     "ILD",  "",     "AY l d"},
    {"",     "IGN",  " ",    "AY n"},
    {"",     "IGN",  "^",    "AY n"},
    {"",     "IGN",  "%",    "AY n"},
    {"",     "IQUE", "",     "IY k"},
    {"",     "I",    "",     "IH"},
    {"",     NULL,   "",     NULL}
};

static const nrl_rule_t rules_j[] = {
    {"",   "J",     "",      "j"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_k[] = {
    {" ",  "K",     "N",     ""},
    {"",   "K",     "",      "k"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_l[] = {
    {"",     "LO",   "C#",   "l OW"},
    {"L",    "L",    "",     ""},
    {"#:^",  "L",    "%",    "AX l"},
    {"",     "LEAD", "",     "l IY d"},
    {"",     "L",    "",     "l"},
    {"",     NULL,   "",     NULL}
};

static const nrl_rule_t rules_m[] = {
    {"",   "MOV",   "",      "m UW v"},
    {"",   "M",     "",      "m"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_n[] = {
    {"E",  "NG",    "+",     "n j"},
    {"",   "NG",    "R",     "NG g"},
    {"",   "NG",    "#",     "NG g"},
    {"",   "NGL",   "%",     "NG g AX l"},
    {"",   "NG",    "",      "NG"},
    {"",   "NK",    "",      "NG k"},
    {" ",  "NOW",   " ",     "n AW"},
    {"",   "N",     "",      "n"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_o[] = {
    {"",     "OF",     " ",   "AX v"},
    {"",     "OROUGH", "",    "ER OW"},
    {"#:",   "ORS",    " ",   "ER z"},
    {"#:",   "OR",     " ",   "ER"},
    {"",     "OR",     "",    "AO r"},
    {" ",    "ONE",    "",    "w AH n"},
    {"",     "OW",     "",    "OW"},
    {" ",    "OVER",   "",    "OW v ER"},
    {"",     "OV",     "",    "AH v"},
    {"",     "O",      "^%",  "OW"},
    {"",     "O",      "^EN", "OW"},
    {"",     "O",      "^I#", "OW"},
    {"",     "OL",     "D",   "OW l"},
    {"",     "OUGHT",  "",    "AO t"},
    {"",     "OUGH",   "",    "AH f"},
    {" ",    "OU",     "",    "AW"},
    {"H",    "OU",     "S#",  "AW"},
    {"",     "OUS",    "",    "AX s"},
    {"",     "OUR",    "",    "AO r"},
    {"",     "OULD",   "",    "UH d"},
    {"^",    "OU",     "^L",  "AH"},
    {"",     "OUP",    "",    "UW p"},
    {"",     "OU",     "",    "AW"},
    {"",     "OY",     "",    "OY"},
    {"",     "OING",   "",    "OW IH NG"},
    {"",     "OI",     "",    "OY"},
    {"",     "OOR",    "",    "AO r"},
    {"",     "OOK",    "",    "UH k"},
    {"",     "OOD",    "",    "UH d"},
    {"",     "OO",     "",    "UW"},
    {"",     "O",      "E",   "OW"},
    {"",     "O",      " ",   "OW"},
    {"",     "OA",     "",    "OW"},
    {" ",    "ONLY",   "",    "OW n l IY"},
    {" ",    "ONCE",   "",    "w AH n s"},
    {"",     "ON'T",   "",    "OW n t"},
    {"C",    "O",      "N",   "AA"},
    {"",     "O",      "NG",  "AO"},
    {" :^",  "O",      "N",   "AH"},
    {"I",    "ON",     "",    "AX n"},
    {"#:",   "ON",     " ",   "AX n"},
    {"#^",   "ON",     "",    "AX n"},
    {"",     "O",      "ST ", "OW"},
    {"",     "OF",     "^",   "AO f"},
    {"",     "OTHER",  "",    "AH DH ER"},
    {"",     "OSS",    " ",   "AO s"},
    {"#:^",  "OM",     "",    "AH m"},
    {"",     "O",      "",    "AA"},
    {"",     NULL,     "",    NULL}
};

static const nrl_rule_t rules_p[] = {
    {"",   "PH",    "",      "f"},
    {"",   "PEOP",  "",      "p IY p"},
    {"",   "POW",   "",      "p AW"},
    {"",   "PUT",   " ",     "p UH t"},
    {"",   "P",     "",      "p"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_q[] = {
    {"",   "QUAR",  "",      "k w AO r"},
    {"",   "QU",    "",      "k w"},
    {"",   "Q",     "",      "k"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_r[] = {
    {" ",  "RE",    "^#",    "r IY"},
    {"",   "R",     "",      "r"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_s[] = {
    {"",      "SH",   "",    "SH"},
    {"#",     "SION", "",    "ZH AX n"},
    {"",      "SOME", "",    "s AH m"},
    {"#",     "SUR",  "#",   "ZH ER"},
    {"",      "SUR",  "#",   "SH ER"},
    {"#",     "SU",   "#",   "ZH UW"},
    {"#",     "SSU",  "#",   "SH UW"},
    {"#",     "SED",  " ",   "z d"},
    {"#",     "S",    "#",   "z"},
    {"",      "SAID", "",    "s EH d"},
    {"^",     "SION", "",    "SH AX n"},
    {"",      "S",    "S",   ""},
    {".",     "S",    " ",   "z"},
    {"#:.E",  "S",    " ",   "z"},
    {"#:^##", "S",    " ",   "z"},
    {"#:^#",  "S",    " ",   "s"},
    {"U",     "S",    " ",   "s"},
    {" :#",   "S",    " ",   "z"},
    {" ",     "SCH",  "",    "s k"},
    {"",      "S",    "C+",  ""},
    {"#",     "SM",   "",    "z m"},
    {"#",     "SN",   "'",   "z AX n"},
    {"",      "S",    "",    "s"},
    {"",      NULL,   "",    NULL}
};

static const nrl_rule_t rules_t[] = {
    {" ",   "THE",     " ",   "DH AX"},
    {"",    "TO",      " ",   "t UW"},
    {"",    "THAT",    " ",   "DH AE t"},
    {" ",   "THIS",    " ",   "DH IH s"},
    {" ",   "THEY",    "",    "DH EY"},
    {" ",   "THERE",   "",    "DH EH r"},
    {"",    "THER",    "",    "DH ER"},
    {"",    "THEIR",   "",    "DH EH r"},
    {" ",   "THAN",    " ",   "DH AE n"},
    {" ",   "THEM",    " ",   "DH EH m"},
    {"",    "THESE",   " ",   "DH IY z"},
    {" ",   "THEN",    "",    "DH EH n"},
    {"",    "THROUGH", "",    "TH r UW"},
    {"",    "THOSE",   "",    "DH OW z"},
    {"",    "THOUGH",  " ",   "DH OW"},
    {" ",   "THUS",    "",    "DH AH s"},
    {"",    "TH",      "",    "TH"},
    {"#:",  "TED",     " ",   "t IH d"},
    {"S",   "TI",      "#N",  "CH"},
    {"",    "TI",      "O",   "SH"},
    {"",    "TI",      "A",   "SH"},
    {"",    "TIEN",    "",    "SH AX n"},
    {"",    "TUR",     "#",   "CH ER"},
    {"",    "TU",      "A",   "CH UW"},
    {" ",   "TWO",     "",    "t UW"},
    {"",    "T",       "",    "t"},
    {"",    NULL,      "",    NULL}
};

static const nrl_rule_t rules_u[] = {
    {" ",   "UN",   "I",    "y UW n"},
    {" ",   "UN",   "",     "AH n"},
    {" ",   "UPON", "",     "AX p AO n"},
    {"@",   "UR",   "#",    "UH r"},
    {"",    "UR",   "#",    "y UH r"},
    {"",    "UR",   "",     "ER"},
    {"",    "U",    "^ ",   "AH"},
    {"",    "U",    "^^",   "AH"},
    {"",    "UY",   "",     "AY"},
    {" G",  "U",    "#",    ""},
    {"G",   "U",    "%",    ""},
    {"G",   "U",    "#",    "w"},
    {"#N",  "U",    "",     "y UW"},
    {"@",   "U",    "",     "UW"},
    {"",    "U",    "",     "y UW"},
    {"",    NULL,   "",     NULL}
};

static const nrl_rule_t rules_v[] = {
    {"",   "VIEW",  "",      "v y UW"},
    {"",   "V",     "",      "v"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_w[] = {
    {" ",  "WERE",  "",      "w ER"},
    {"",   "WA",    "S",     "w AA"},
    {"",   "WA",    "T",     "w AA"},
    {"",   "WHERE", "",      "WH EH r"},
    {"",   "WHAT",  "",      "WH AA t"},
    {"",   "WHOL",  "",      "h OW l"},
    {"",   "WHO",   "",      "h UW"},
    {"",   "WH",    "",      "WH"},
    {"",   "WAR",   "",      "w AO r"},
    {"",   "WOR",   "^",     "w ER"},
    {"",   "WR",    "",      "r"},
    {"",   "W",     "",      "w"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_x[] = {
    {"",   "X",     "",      "k s"},
    {"",   NULL,    "",      NULL}
};

static const nrl_rule_t rules_y[] = {
    {"",     "YOUNG", "",     "y AH NG"},
    {" ",    "YOU",   "",     "y UW"},
    {" ",    "YES",   "",     "y EH s"},
    {" ",    "Y",     "",     "y"},
    {"#:^",  "Y",     " ",    "IY"},
    {"#:^",  "Y",     "I",    "IY"},
    {" :",   "Y",     " ",    "AY"},
    {" :",   "Y",     "#",    "AY"},
    {" :",   "Y",     "^+:#", "IH"},
    {" :",   "Y",     "^#",   "AY"},
    {"",     "Y",     "",     "IH"},
    {"",     NULL,    "",     NULL}
};

static const nrl_rule_t rules_z[] = {
    {"",   "Z",     "",      "z"},
    {"",   NULL,    "",      NULL}
};

/* Digits are spoken as the words, which the rules themselves then handle --
   so "1976" reads as four digits, not as a year. That is what every NRL
   implementation does and it is the honest behaviour for a letter-to-sound
   engine with no number grammar. */
static const char *const digit_words[10] = {
    "ZERO", "ONE", "TWO", "THREE", "FOUR",
    "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"
};

static const nrl_rule_t *const rule_groups[26] = {
    rules_a, rules_b, rules_c, rules_d, rules_e, rules_f, rules_g,
    rules_h, rules_i, rules_j, rules_k, rules_l, rules_m, rules_n,
    rules_o, rules_p, rules_q, rules_r, rules_s, rules_t, rules_u,
    rules_v, rules_w, rules_x, rules_y, rules_z
};

/* --------------------------------------------------------- the exceptions
   Taken from animated-speaking-cpc-vector-head's head.asm, where each entry
   was measured against the 1985 engine rather than guessed -- then RE-measured
   here, because that engine held a cut-down ruleset in 5170 bytes of Z80 and
   this is the full 1976 one. Seventeen of its thirty-five entries turned out
   to be unnecessary or harmful and are gone:

     HEAD BROUGHT SAID DOES MOTHER WHO GREAT BUILD THOUGHT THROUGH
         the rules get these right on their own; those ten were only ever
         working around what 1985 left out.
     ONCE WHERE PEOPLE AGAIN DIGITAL DIGITIZE DIGITIZED
         the rules get these right and the respelling made them WORSE -- WUNS
         voices the final s to a z, WAIR loses the WH, and DIJJI TIZED puts a
         word boundary in the middle of a word.

   Two respellings had to change: HY and HY YA relied on Y counting as a vowel
   after H, which it does not here, so the H was deleted and HI came out as
   "eye". HIGH and HIGH YA say it.

   The respelling is fed to the rules in place of the word, so these cost no
   rule changes and each can be checked on its own with host/nrl_test. */
typedef struct { const char *word, *said; } nrl_fix_t;

static const nrl_fix_t fixes[] = {
    {"LIVE","LIV"},          {"HI","HIGH"},          {"HIYA","HIGH YA"},
    {"HEY","HAI"},           {"OH","OWE"},           {"BYE","BY"},
    {"GOODBYE","GUD BY"},    {"DIGIT","DIJJIT"},     {"SAYS","SEZ"},
    {"MACHINE","MUSHEEN"},   {"FRIEND","FREND"},     {"HEART","HART"},
    {"BUSY","BIZZY"},        {"WOMEN","WIMMIN"},     {"ENOUGH","INUFF"},
    {"LAUGH","LAF"},         {"MICHAEL","MIKEL"},    {"SCHNEIDER","SHNYDER"},

    /* Added here, measured the same way. All five are the rules working
       exactly as the report specifies and still being wrong:
         ABOUT   initial unstressed A has no rule, so it comes out stressed.
                 NRL leaves it that way on purpose -- nothing in the spelling
                 says whether the A of ABOUT or of APPLE carries the syllable.
         YOUR    the YOU rule has no right context, so YOUR is "you-er".
         VERY    ER before Y is not ER before a vowel, so it takes the ER of
                 "fir" instead of the EH r of "merry".
         WANT    WA is only backed off to "wah" before S and T.
         PROBLEM the suffix class % includes a bare E, so the EM of PROBLEM
                 reads as a suffix and earns an extra schwa.
       BROWN is deliberately NOT here: OW defaults to the vowel of "know" and
       the report patches NOW, HOW, DOW and POW one at a time. English spells
       both sounds the same way, and no respelling reaches the AW of "out"
       without splitting the word. */
    {"ABOUT","ERBOUT"},      {"YOUR","YOR"},         {"VERY","VEHRY"},
    {"WANT","WAUNT"},        {"PROBLEM","PROBLUM"}
};
#define N_FIXES ((int)(sizeof fixes / sizeof fixes[0]))

static int fixes_enabled = 1;
void        nrl_set_exceptions(int on)     { fixes_enabled = on ? 1 : 0; }
int         nrl_exception_count(void)      { return N_FIXES; }
const char *nrl_exception_word(int i)      { return (i>=0 && i<N_FIXES) ? fixes[i].word : NULL; }
const char *nrl_exception_said(int i)      { return (i>=0 && i<N_FIXES) ? fixes[i].said : NULL; }

/* ----------------------------------------------------------------- stage 1 */

#define NRL_WORK_MAX 320u

/* Uppercase, and substitute the exception table a word at a time. A word is
   what stands between characters that are not letters, so a full stop after a
   word does not hide it from the table. */
static void prepare(const char *text, char *w, size_t cap)
{
    size_t o = 0u;
    size_t i = 0u;

    while (text[i] != '\0' && o + 1u < cap) {
        const unsigned char c = (unsigned char)text[i];
        if (isalpha(c)) {
            size_t n = 0u;
            while (isalpha((unsigned char)text[i + n])) ++n;

            char word[32];
            if (n < sizeof word) {
                for (size_t k = 0u; k < n; ++k) {
                    word[k] = (char)toupper((unsigned char)text[i + k]);
                }
                word[n] = '\0';

                const char *said = NULL;
                for (int f = 0; fixes_enabled && f < N_FIXES; ++f) {
                    if (!strcmp(word, fixes[f].word)) { said = fixes[f].said; break; }
                }
                if (said != NULL) {
                    for (size_t k = 0u; said[k] != '\0' && o + 1u < cap; ++k) w[o++] = said[k];
                    i += n;
                    continue;
                }
            }
            for (size_t k = 0u; k < n && o + 1u < cap; ++k) {
                w[o++] = (char)toupper((unsigned char)text[i + k]);
            }
            i += n;
            continue;
        }

        if (isdigit(c)) {
            const char *d = digit_words[c - '0'];
            if (o + 1u < cap) w[o++] = ' ';
            for (size_t k = 0u; d[k] != '\0' && o + 1u < cap; ++k) w[o++] = d[k];
            if (o + 1u < cap) w[o++] = ' ';
            ++i;
            continue;
        }

        w[o++] = (char)c;
        ++i;
    }
    w[o] = '\0';
}

/* Append one phoneme token. */
typedef struct { char *buf; size_t cap, len; int truncated; } out_t;

static void emit(out_t *o, const char *tok, size_t n)
{
    if (n == 0u) return;
    if (o->len != 0u) {
        if (o->len + 1u >= o->cap) { o->truncated = 1; return; }
        o->buf[o->len++] = ' ';
    }
    if (o->len + n >= o->cap) { o->truncated = 1; return; }
    memcpy(o->buf + o->len, tok, n);
    o->len += n;
}

static void emit_list(out_t *o, const char *list)
{
    const char *p = list;
    while (*p != '\0') {
        while (*p == ' ') ++p;
        const char *s = p;
        while (*p != '\0' && *p != ' ') ++p;
        emit(o, s, (size_t)(p - s));
    }
}

/* The working buffers are static, not automatic. An RP2040 core gets 2 KB of
   stack by default and this chain used 1708 bytes of it -- the same trap the
   vendored core's interactive debugger fell into with its 1112-byte frame, and
   it is just as silent: the first text-mode utterance overflows into the other
   core's stack and the card goes quiet with no fault reported.

   The cost is that neither entry point is reentrant and both must be called
   from one core only. That is already the contract: pp2_sp_say_english() is
   producer-side, which means the Apple II / USB core and nothing else. */
static char nrl_work[NRL_WORK_MAX];
static char nrl_arpa[NRL_WORK_MAX * 3u];

size_t nrl_text_to_arpa(const char *text, char *out, size_t cap)
{
    if (cap == 0u) return 0u;
    out[0] = '\0';
    if (text == NULL) return 0u;

    char *const work = nrl_work;
    prepare(text, work, sizeof nrl_work);

    out_t o = { out, cap, 0u, 0 };
    int i = 0;

    while (work[i] != '\0') {
        const char c = work[i];
        const nrl_rule_t *group;

        if (is_letter(c)) {
            group = rule_groups[c - 'A'];
        } else {
            group = rules_punct;
        }

        int advanced = 0;
        for (const nrl_rule_t *r = group; r->match != NULL; ++r) {
            const size_t n = strlen(r->match);
            if (strncmp(work + i, r->match, n) != 0) continue;
            if (!match_left(work, i, r->left)) continue;
            if (!match_right(work, i + (int)n, r->right)) continue;

            if (r->out[0] == ' ') {
                emit(&o, "_", 1u);      /* word boundary */
            } else {
                emit_list(&o, r->out);
            }
            i += (int)n;
            advanced = 1;
            break;
        }
        if (!advanced) ++i;             /* unmatchable character: skip it */
    }

    out[o.len] = '\0';
    return o.len;
}

/* ----------------------------------------------------------------- stage 2
   ARPAbet to SP0256-AL2. This is where the chip's positional variants are
   chosen, and the choices follow the data sheet's own example words:

     KK1 "can't"   initial before a front vowel      KK2 "sky"   final or in a
     KK3 "comb"    initial before a back vowel                   cluster
     GG1 "got"     initial before a back vowel       GG2 "guest" before front
     GG3 "wig"     final                             TT1 "part"  final
     TT2 "to"      elsewhere                         DD1 "could" final
     DD2 "do"      initial                           BB1 initial, BB2 final
     HH1 "he"      before a front vowel              HH2 "hoe"   before back
     NN2 "no"      initial                           NN1 "thin"  elsewhere
     RR1 "rural"   initial                           RR2 "brain" after a
     DH1 "they"    initial                           DH2 "bathe" consonant
     EL  "saddle"  syllabic l                        LL  "lake"  elsewhere

   Two things matter more than any of those. The vowel+r allophones -- OR, AR,
   YR, XR -- exist because /r/ after a vowel is what the chip is worst at, and
   using them instead of vowel+RR is the single largest gain in intelligibility.
   And a plosive needs a pause in front of it or its burst is inaudible: PA2
   before a word-initial one, PA1 inside a word. */

enum {
  P_IY, P_IH, P_EY, P_EH, P_AE, P_AA, P_AO, P_OW, P_UH, P_UW, P_ER, P_AX,
  P_AH, P_AY, P_AW, P_OY,
  P_p, P_b, P_t, P_d, P_k, P_g, P_f, P_v, P_TH, P_DH, P_s, P_z, P_SH, P_ZH,
  P_h, P_m, P_n, P_NG, P_l, P_w, P_y, P_r, P_CH, P_j, P_WH,
  P_BOUND, P_NONE
};

static const char *const phon_name[] = {
  "IY","IH","EY","EH","AE","AA","AO","OW","UH","UW","ER","AX",
  "AH","AY","AW","OY",
  "p","b","t","d","k","g","f","v","TH","DH","s","z","SH","ZH",
  "h","m","n","NG","l","w","y","r","CH","j","WH",
  "_"
};

/* AL2 codes, by the data sheet's numbering. */
enum {
  A_PA1=0, A_PA2=1, A_PA3=2, A_PA4=3, A_PA5=4, A_OY=5, A_AY=6, A_EH=7,
  A_KK3=8, A_PP=9, A_JH=10, A_NN1=11, A_IH=12, A_TT2=13, A_RR1=14, A_AX=15,
  A_MM=16, A_TT1=17, A_DH1=18, A_IY=19, A_EY=20, A_DD1=21, A_UW1=22, A_AO=23,
  A_AA=24, A_YY2=25, A_AE=26, A_HH1=27, A_BB1=28, A_TH=29, A_UH=30, A_UW2=31,
  A_AW=32, A_DD2=33, A_GG3=34, A_VV=35, A_GG1=36, A_SH=37, A_ZH=38, A_RR2=39,
  A_FF=40, A_KK2=41, A_KK1=42, A_ZZ=43, A_NG=44, A_LL=45, A_WW=46, A_XR=47,
  A_WH=48, A_YY1=49, A_CH=50, A_ER1=51, A_ER2=52, A_OW=53, A_DH2=54, A_SS=55,
  A_NN2=56, A_HH2=57, A_OR=58, A_AR=59, A_YR=60, A_GG2=61, A_EL=62, A_BB2=63
};

static int phon_lookup(const char *tok, size_t n)
{
    for (int i = 0; i <= P_BOUND; ++i) {
        if (strlen(phon_name[i]) == n && !strncmp(phon_name[i], tok, n)) return i;
    }
    return P_NONE;
}

static int is_vowel_phon(int p) { return p >= P_IY && p <= P_OY; }
static int is_front_vowel(int p)
{
    return p==P_IY || p==P_IH || p==P_EY || p==P_EH || p==P_AE || p==P_AY;
}
static int is_plosive(int p)
{
    return p==P_p || p==P_b || p==P_t || p==P_d || p==P_k || p==P_g ||
           p==P_CH || p==P_j;
}
static int is_voiced_plosive(int p) { return p==P_b || p==P_d || p==P_g || p==P_j; }

size_t nrl_text_to_al2(const char *text, uint8_t *out, size_t cap)
{
    char *const arpa = nrl_arpa;
    nrl_text_to_arpa(text, arpa, sizeof nrl_arpa);

    /* Tokenise into a phoneme array first, because stage 2 needs to look both
       ways: at the next phoneme to pick a variant, and at the previous one to
       know whether a word has just started. */
    static int ph[NRL_WORK_MAX];
    int n = 0;
    const char *p = arpa;
    while (*p != '\0' && n < (int)(sizeof ph / sizeof ph[0])) {
        while (*p == ' ') ++p;
        if (*p == '\0') break;
        const char *s = p;
        while (*p != '\0' && *p != ' ') ++p;
        const int id = phon_lookup(s, (size_t)(p - s));
        if (id != P_NONE) ph[n++] = id;
    }

    /* A respelling that doubles a consonant to force a short vowel -- BIZZY,
       WIMMIN, DIJJIT, the trick the exception table is built on -- also asks
       the chip to say that consonant twice, which it audibly does. English has
       no geminate consonant inside a word, so drop the repeat. A word boundary
       between the two survives as a _ and is not a repeat, so BUS STOP keeps
       both. */
    {
        int w = 0;
        for (int i = 0; i < n; ++i) {
            if (w > 0 && ph[i] == ph[w-1] && !is_vowel_phon(ph[i])) {
                continue;   /* also collapses ". " into one boundary */
            }
            ph[w++] = ph[i];
        }
        n = w;
    }

    size_t o = 0u;
    #define PUT(c) do { if (o < cap) out[o] = (uint8_t)(c); ++o; } while (0)

    for (int i = 0; i < n; ++i) {
        const int cur  = ph[i];
        const int prev = (i > 0)     ? ph[i-1] : P_BOUND;
        const int next = (i + 1 < n) ? ph[i+1] : P_BOUND;
        const int initial = (prev == P_BOUND);
        const int final   = (next == P_BOUND);

        if (cur == P_BOUND) { PUT(A_PA3); continue; }

        /* Vowel + r: use the chip's own combined allophones and swallow the r.
           Checked before anything else because it replaces two phonemes with
           one allophone.

           Only when the r closes the syllable. An r between two vowels is the
           next syllable's onset and stays a real RR -- otherwise VERY becomes
           "vairy" and SORRY becomes "sarry", because XR and AR are r-coloured
           vowels rather than a vowel followed by an r. */
        if (is_vowel_phon(cur) && next == P_r &&
            !(i + 2 < n && is_vowel_phon(ph[i+2]))) {
            switch (cur) {
            case P_AO: case P_OW: PUT(A_OR); ++i; continue;
            case P_AA: case P_AH: PUT(A_AR); ++i; continue;
            case P_IY: case P_IH: PUT(A_YR); ++i; continue;
            case P_EH: case P_EY: case P_AE: PUT(A_XR); ++i; continue;
            default: break;   /* AY/AW/OY/UW keep a real r after them */
            }
        }

        if (is_plosive(cur)) {
            PUT(initial ? A_PA2 : A_PA1);
        }

        switch (cur) {
        case P_IY: PUT(A_IY); break;
        case P_IH: PUT(A_IH); break;
        case P_EY: PUT(A_EY); break;
        case P_EH: PUT(A_EH); break;
        case P_AE: PUT(A_AE); break;
        case P_AA: PUT(A_AA); break;
        case P_AO: PUT(A_AO); break;
        case P_OW: PUT(A_OW); break;
        case P_UH: PUT(A_UH); break;
        /* UW1 is the short "to", UW2 the long "food". The rules do not mark
           stress, so length comes from position: a final or pre-consonant /u/
           carries the syllable and gets the long one. */
        case P_UW: PUT((final || !is_vowel_phon(next)) ? A_UW2 : A_UW1); break;
        /* ER1 is the unstressed "letter" ending, ER2 the stressed "fir". A
           final ER in a word of more than two phonemes is the suffix. */
        case P_ER: PUT((final && i >= 2) ? A_ER1 : A_ER2); break;
        case P_AX: case P_AH: PUT(A_AX); break;   /* AL2 has no distinct /ʌ/ */
        case P_AY: PUT(A_AY); break;
        case P_AW: PUT(A_AW); break;
        case P_OY: PUT(A_OY); break;

        case P_p: PUT(A_PP); break;
        case P_b: PUT(initial ? A_BB1 : A_BB2); break;
        case P_t: PUT(final ? A_TT1 : A_TT2); break;
        case P_d: PUT(initial ? A_DD2 : A_DD1); break;
        /* KK2 is the data sheet's "sky": the /k/ of a cluster or a final one.
           A /k/ not followed by a vowel is in a cluster (CLEAR) and so is one
           not preceded by a vowel inside a word (SKY); only a /k/ that both
           opens a syllable and leads to a vowel gets KK1 or KK3, chosen by
           whether that vowel is front ("can't") or back ("comb"). */
        case P_k:
            if (!is_vowel_phon(next))                      PUT(A_KK2);
            else if (!initial && !is_vowel_phon(prev))      PUT(A_KK2);
            else if (is_front_vowel(next))                 PUT(A_KK1);
            else                                           PUT(A_KK3);
            break;
        case P_g:
            if (final)                   PUT(A_GG3);
            else if (is_front_vowel(next)) PUT(A_GG2);
            else                         PUT(A_GG1);
            break;
        case P_f:  PUT(A_FF); break;
        case P_v:  PUT(A_VV); break;
        case P_TH: PUT(A_TH); break;
        case P_DH: PUT(initial ? A_DH1 : A_DH2); break;
        case P_s:  PUT(A_SS); break;
        case P_z:  PUT(A_ZZ); break;
        case P_SH: PUT(A_SH); break;
        case P_ZH: PUT(A_ZH); break;
        case P_h:  PUT(is_front_vowel(next) ? A_HH1 : A_HH2); break;
        case P_m:  PUT(A_MM); break;
        case P_n:  PUT(initial ? A_NN2 : A_NN1); break;
        case P_NG: PUT(A_NG); break;
        /* EL is the syllabic l of "saddle": word-final after a consonant. */
        case P_l:  PUT((final && !is_vowel_phon(prev)) ? A_EL : A_LL); break;
        case P_w:  PUT(A_WW); break;
        case P_WH: PUT(A_WH); break;
        case P_y:  PUT(initial ? A_YY2 : A_YY1); break;
        /* RR2 is the "brain" r, inside a consonant cluster. */
        case P_r:  PUT((!initial && !is_vowel_phon(prev)) ? A_RR2 : A_RR1); break;
        case P_CH: PUT(A_CH); break;
        case P_j:  PUT(A_JH); break;
        default: break;
        }
    }

    PUT(A_PA5);   /* let the last sound finish before the queue goes idle */
    #undef PUT
    return (o <= cap) ? o : cap;
}
