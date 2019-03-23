#!/bin/bash
wav="$1"
ffmpeg -i "$1" -acodec aac -ab 64000 -ac 1 -ar 32000 -movflags faststart -y "$1.m4a"

