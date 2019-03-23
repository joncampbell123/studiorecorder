#!/bin/bash
wav="$1"
lame -b 8 -V 0 --preset extreme -a -m m -q 0 --abr 64 -B 80 --resample 44.1 "$1" "$1.64k.mp3"
lame -b 8 -V 0 --preset extreme -a -m m -q 0 --abr 48 -B 64 --resample 32 "$1" "$1.48k.mp3"
lame -b 8 -V 0 --preset extreme -a -m m -q 0 --abr 24 -B 32 --resample 16 "$1" "$1.24k.mp3"

