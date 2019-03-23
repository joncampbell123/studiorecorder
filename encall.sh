#!/bin/bash
wav="$1"
for i in enc-*.sh; do
    if [ -x "./$i" ]; then
        "./$i" "$wav" || exit 1
    fi
done

