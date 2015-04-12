#!/bin/bash
find data \( -name '*.cfg' \) -print0 | xargs -0 ./wesnoth
