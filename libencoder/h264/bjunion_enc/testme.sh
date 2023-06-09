#!/bin/sh
FILENAME=/tmp/$$.ctl
echo $FILENAME
dd if=/dev/random of=$FILENAME bs=1 count=2
exec ./testmem
