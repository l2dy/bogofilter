#!/bin/sh
(
export LANG=en LC_ALL=C
date --utc --iso-8601=seconds 
command time ./do-conversion.sh
) 2>&1 | tee conversion.log
