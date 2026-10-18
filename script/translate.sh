#!/bin/bash

set -e

# Finished = translated messages that are not unfinished, vanished or obsolete.
finished_count() {
    { grep -o '<translation[^>]*>' "$1" || true; } | { grep -vcE 'type="(unfinished|vanished|obsolete)"' || true; }
}

ts_files=(res/translations/*.ts)
before=()
for i in "${!ts_files[@]}"; do
    before[$i]=$(finished_count "${ts_files[$i]}")
done

cmake --build ./build --target update_translations

dropped=0
for i in "${!ts_files[@]}"; do
    ts=${ts_files[$i]}
    after=$(finished_count "$ts")
    if [ "$after" -lt "${before[$i]}" ]; then
        echo "translate.sh: $ts finished translations dropped from ${before[$i]} to $after (a tr() context or source string changed)" >&2
        dropped=1
    fi
done
exit $dropped
