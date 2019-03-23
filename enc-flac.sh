#!/bin/bash
wav="$1"
ffmpeg -i "$1" -acodec flac -ac 1 -ar 32000 -y "$1.flac"

