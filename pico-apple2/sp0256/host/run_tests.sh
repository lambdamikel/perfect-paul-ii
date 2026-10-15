#!/bin/sh
# Perfect Paul ][ SP0256 firmware: the checks that must hold before flashing.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# These run the firmware's OWN protocol and voice code, so a failure here is a
# failure on the card. Run from this directory after `make`.
# NO set -e. Several checks deliberately run a command that exits non-zero --
# a rejected token must exit 2 -- and `set -e` is inherited by the $( )
# subshell, so it kills the check before `echo $?` can report the status. The
# symptom is the suite stopping after three tests and never saying ALL PASS.
cd "$(dirname "$0")" || exit 1
fail=0
chk() { r=$(eval "$2"); if [ "$r" = "$3" ]; then echo "  pass  $1";
        else echo "  FAIL  $1: got [$r] want [$3]"; fail=1; fi; }

echo "protocol:"
chk "names == numbers == raw POKEs" \
  './pp2_render -a -o /tmp/t1.wav "HH1 EH LL OW" >/dev/null; ./pp2_render -a -o /tmp/t2.wav "27 7 45 53" >/dev/null; ./pp2_render -o /tmp/t3.wav -p 219,199,237,245 >/dev/null; md5sum /tmp/t1.wav /tmp/t2.wav /tmp/t3.wav | awk "{print \$1}" | sort -u | wc -l' 1
chk "case insensitive" './pp2_render -a -o /tmp/t4.wav "hh1 eh ll ow" >/dev/null; cmp -s /tmp/t1.wav /tmp/t4.wav && echo same' same
chk "stop byte silences" './pp2_render -q -p 219,199,237,245,144 | grep -c "peak=0"' 1
chk "bad tokens rejected, exit 2" './pp2_render -a -q "XX ZZZZ 99" >/dev/null 2>&1; echo $?' 2
chk "long line not split mid-token" './pp2_render -a -q "$(python3 -c "print(\" \".join([\"PA1\"]*45))")" | grep -oE "allophones=45"' allophones=45
chk "stage overflow is counted" './pp2_render -a -q "$(python3 -c "print(\" \".join([\"AA\"]*5000))")" | grep -oE "dropped=[0-9]+" | grep -cv "dropped=0"' 1
chk "mode byte 177 switches to allophones" './pp2_render -q -p 177,72,72,49,32,69,72,32,76,76,32,79,87,13 | grep -oE "allophones=4"' allophones=4
chk "text mode is the default" './pp2_render -q "HELLO THERE" | grep -oE "allophones=8"' allophones=8

# The NRL rules are validated by the mistakes they are DOCUMENTED to make. A
# ruleset that gets HEAD and HI right with the exception table off is not this
# ruleset -- see nrl.h and the vector-head project's measurements.
# The one that only hardware found. Both host renderers push everything up
# front and stop at idle, so until this existed nothing ever asked the chip to
# speak twice -- and it deadlocked the second time, every time.
echo "the chip must speak more than once:"
chk "a second utterance after idle" './second_utterance >/dev/null 2>&1; echo $?' 0

echo "NRL rules -- the documented 1976 errors must survive:"
chk "HEAD unaided (full ruleset gets it right)" './nrl_test -x HEAD | grep -c "h EH d"' 1
chk "HI unaided is hih" './nrl_test -x HI | grep -c "h IH"' 1
chk "LIVE unaided is the adjective" './nrl_test -x LIVE | grep -c "l AY v"' 1
chk "exception table fixes HI" './nrl_test HI | grep -c "h AY"' 1
chk "geminates collapse" './nrl_test BUSY | sed -n "s/^  al2 : //p" | grep -c "BB1 IH ZZ IY"' 1
chk "word boundary keeps both" './nrl_test "BUS STOP" | sed -n "s/^  al2 : //p" | grep -c "SS PA3 SS"' 1
chk "syllable-final r collapses to OR" './nrl_test MORE | sed -n "s/^  al2 : //p" | grep -c "MM OR"' 1
chk "intervocalic r stays RR1" './nrl_test VERY | sed -n "s/^  al2 : //p" | grep -c "EH RR1 IY"' 1
chk "k in a cluster is KK2" './nrl_test CLEAR | grep -c "KK2 LL YR"' 1
chk "k before a back vowel is KK3" './nrl_test COMB | grep -c "KK3"' 1
chk "k before a front vowel is KK1" "./nrl_test \"CAN'T\" | grep -c KK1" 1

echo
[ $fail -eq 0 ] && echo "ALL PASS" || { echo "FAILURES PRESENT"; exit 1; }
