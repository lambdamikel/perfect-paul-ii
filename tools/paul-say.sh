#!/usr/bin/env bash
#
# paul-say.sh - talk to a Perfect Paul ][ card over its USB CDC console.
#
#   ./paul-say.sh "Hello from the terminal."    speak one line
#   ./paul-say.sh                               interactive; type lines, Ctrl-D to quit
#   ./paul-say.sh -l                            just listen to the card's output
#   PORT=/dev/ttyACM1 ./paul-say.sh             override the port
#
# The firmware accepts the same line protocol on USB CDC as it does on the
# Apple II slot: a carriage return speaks the line, and 0x03 stops speech. That
# makes this the fastest way to audition DECtalk phrasing - including phoneme
# spellings - without rebuilding and reflashing.
#
# It exists because minicom defaults hardware flow control to on, the Pico's
# CDC never asserts CTS, and the result is a card that silently ignores
# everything you type and looks dead.
#
# SPDX-License-Identifier: MIT

set -u

if [ -n "${PORT:-}" ]; then
    if [ ! -e "$PORT" ]; then
        echo "$PORT does not exist." >&2
        exit 1
    fi
else
    for candidate in /dev/ttyACM*; do
        [ -e "$candidate" ] && PORT="$candidate" && break
    done
    if [ -z "${PORT:-}" ]; then
        echo "No USB CDC port found. Is the card plugged in and running the firmware?" >&2
        echo "Looked for /dev/ttyACM*. Override with: PORT=/dev/ttyXXX $0" >&2
        exit 1
    fi
fi

if [ ! -w "$PORT" ]; then
    echo "$PORT is not writable. Add yourself to the 'dialout' group:" >&2
    echo "  sudo usermod -aG dialout \$USER   (then log out and back in)" >&2
    exit 1
fi

# Raw, no echo, and no hangup-on-close so the card is not reset when we exit.
# Never open this port at 1200 baud: that is the Pico's BOOTSEL-reset trigger
# and it would drop into the bootloader instead of speaking. USB CDC ignores the
# rate itself, so 115200 is arbitrary but safe.
stty -F "$PORT" raw -echo -hupcl clocal 115200 || exit 1

if [ "${1:-}" = "-l" ] || [ "${1:-}" = "--listen" ]; then
    echo "Listening on $PORT. Ctrl-C to stop."
    exec cat "$PORT"
fi

exec 3>"$PORT"

if [ $# -ge 1 ]; then
    printf '%s\r' "$*" >&3
    exit 0
fi

echo "Connected to $PORT. Type a line and press Enter; Ctrl-D to quit."
echo "Phoneme example:"
echo "  [:phone arpa speak on][prrfihkt][:phone arpa speak off] Paul Two ready."
echo
while IFS= read -r -p "paul> " line; do
    printf '%s\r' "$line" >&3
done
echo
