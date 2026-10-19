#!/bin/bash
# usage: gen_one.sh WEEK NAME DESCRIPTION
week="$1"; name="$2"; desc="$3"; out="/private/tmp/plate_gen/raw/week-$week-$name.png"
[ -s "$out" ] && exit 0
work="/private/tmp/plate_gen/work/$week"; mkdir -p "$work"
for try in 1 2; do
  codex exec --skip-git-repo-check -s workspace-write -C "$work" "Use your image generation tool to create ONE image and save it as plate.png in the current directory (PNG, square 1:1). Object: $desc. $(cat /private/tmp/plate_gen/style.txt) When done, reply with only the saved file path." >"$work/log.txt" 2>&1
  [ -s "$work/plate.png" ] && cp "$work/plate.png" "$out" && echo "ok $week $name" && exit 0
done
echo "FAILED $week $name"
