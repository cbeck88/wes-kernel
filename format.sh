#!/bin/bash
set -e
for f in `find src -maxdepth 1 \( -name '*.?pp' \) -print0 | xargs -0`
do
  if [ -f "src/temp" ]; then 
    rm src/temp
  fi
  clang-format-3.5 --style=file $f > src/temp
  mv src/temp $f
done
