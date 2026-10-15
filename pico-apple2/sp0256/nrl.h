/* English text to SP0256-AL2 allophones: the NRL letter-to-sound rules.
   SPDX-License-Identifier: GPL-3.0-or-later

   The rules are NRL Report 7948, "Automatic Translation of English Text to
   Phonetic Codes" (Elovitz, Johnson, McHugh and Shore, Naval Research
   Laboratory, 1976) -- a US Government work. They are reimplemented here from
   the report's own notation rather than taken from any binary: the version
   Amstrad shipped in 1985 as the SSA-1's |SAY is a relocatable Z80 blob with
   no licence attached, and is not reusable.

   Two stages, each separately testable:

     1. letter-to-sound, producing ARPAbet phonemes. This is the NRL algorithm
        proper: longest-match rules, scanned by first letter, each with a left
        and right context.
     2. phoneme-to-allophone, which is where the SP0256's positional variants
        get chosen -- KK1/KK2/KK3, TT1/TT2, the vowel+r allophones -- and where
        the pauses that make a plosive audible get inserted.

   The rules have no exception dictionary and English is English, so a small
   table of respellings runs first. Every entry in it was MEASURED against the
   1985 engine on an emulated CPC rather than guessed, in the
   animated-speaking-cpc-vector-head project; it is reused here unchanged. */
#ifndef NRL_H
#define NRL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert English text to SP0256-AL2 allophone codes (0..63).
   Returns the number written. Never writes more than cap. */
size_t nrl_text_to_al2(const char *text, uint8_t *out, size_t cap);

/* Stage 1 alone, as a space-separated ARPAbet string -- what the tests and
   the host tool compare against. Always NUL-terminates when cap > 0. */
size_t nrl_text_to_arpa(const char *text, char *out, size_t cap);

/* The exception table can be switched off, which is how the rules get checked
   against the mistakes they are DOCUMENTED to make: with it off, HEAD must
   come out as "heed" and HI as "hih". A ruleset that gets those right is not
   the NRL ruleset. Default is on. */
void nrl_set_exceptions(int on);

/* How many respellings the exception table holds, and what they are; the host
   tool prints them so the table can be reviewed without reading the source. */
int         nrl_exception_count(void);
const char *nrl_exception_word(int i);
const char *nrl_exception_said(int i);

#ifdef __cplusplus
}
#endif
#endif
