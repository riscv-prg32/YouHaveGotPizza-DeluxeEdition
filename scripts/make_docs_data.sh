#!/usr/bin/env bash
# Regenerate every number and medium quoted by the documentation.
#
#   scripts/make_docs_data.sh
#
# docs/data/balance.csv        Monte Carlo study, one row per lesson (1000 games each)
# docs/data/campaign.txt       survival through a whole campaign
# docs/data/audio_metrics.csv  the soundtrack through the firmware's own mixer
# docs/data/spi.txt            pixels sent to the display, by flush strategy
# docs/media/*.m4a             stereo renders of the three songs
# docs/media/*.png             screens from the host tests' panel model
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
prg32="$(cd "${PRG32_REPO:-$root/../PRG32}" && pwd)"
build="$root/build"
mkdir -p "$build/renders" "$root/docs/data" "$root/docs/media"
cd "$root"

cc -std=c99 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-const-variable -O2 -I src \
   tests/balance.c -o "$build/balance"
"$build/balance" 1000 > docs/data/balance.csv 2> docs/data/campaign.txt

tests/run_tests.sh shots | tee "$build/tests.log" | grep -E "^spi:" > docs/data/spi.txt
for n in title lesson1 lesson2 lesson5 card ending gameover pause; do
  cp "build/shots/$n.png" "docs/media/$n.png"
done

python3 "$prg32/tools/prg32audio_pack.py" audio/audio.json --out "$build/audio.block" >/dev/null
a="$prg32/components/prg32_audio"
cc -std=gnu11 -O2 -Wno-unused-function -Wno-unused-const-variable -I tests/host/stubs -I "$a" -I "$a/include" -I src \
   tests/render_music.c "$a/audio_mixer.c" "$a/audio_synth.c" -lm -o "$build/render_music"
echo "song,peak,rms,stereo_correlation,side_to_mid,left_right_ratio" > docs/data/audio_metrics.csv
for s in "0 40 title" "1 34 piazza" "2 28 mercato"; do
  # shellcheck disable=SC2086
  set -- $s
  "$build/render_music" "$build/audio.block" "$1" "$2" "$build/renders/$3.wav" | sed "s/^$1,/$3,/" >> docs/data/audio_metrics.csv
  ffmpeg -y -loglevel error -i "$build/renders/$3.wav" -c:a aac -b:a 96k "docs/media/$3.m4a"
done
echo "docs/data and docs/media regenerated"
