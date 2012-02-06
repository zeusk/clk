#!/bin/sh
rm -rf ./lk
mkdir ./lk
rsync -av --exclude='*lk-build*' --exclude="../.git" ../ ./lk/
./compile clean