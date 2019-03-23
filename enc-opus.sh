#!/bin/bash
wav="$1"
opusenc --bitrate 64.0 --downmix-mono "$1" "$1.opus"

