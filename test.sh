#!/bin/bash
set -e
for f in `find data \( -name '*.cfg' \) -print0 | xargs -0`
do
  ./wesnoth $f
done
