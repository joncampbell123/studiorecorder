#!/bin/bash
wav="$1"
ffmpeg -i "$1" -acodec wmav2 -ab 64000 -ac 1 -ar 32000 -y "$1.wma"

