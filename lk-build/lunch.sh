#!/bin/sh
rm -rf ./lk
mkdir ./lk
rsync -av --exclude='*lk-build*' ../ ./lk/
./compile clean
rm -rf ./lk