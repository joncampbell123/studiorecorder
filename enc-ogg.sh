#!/bin/bash
wav="$1"
oggenc --resample 32000 --downmix --quality 4 -b 64 "$1" -o "$1.ogg"

