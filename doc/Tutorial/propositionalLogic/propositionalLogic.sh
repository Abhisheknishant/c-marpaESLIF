#!/bin/zsh

pushd /home/jdurand/git/c-marpaESLIF
make marpaESLIF
popd

cc -I include -I output/include -I 3rdparty/github/genericLogger/include -I 3rdparty/github/genericLogger/output/include -o /tmp/jdd doc/Tutorial/propositionalLogic/propositionalLogicUsingLua.c -L output/lib -lmarpaESLIF -L 3rdparty/github/genericLogger/output/lib -lgenericLogger -lm && LD_LIBRARY_PATH=`pwd`/output/lib:`pwd`/3rdparty/github/genericLogger/output/lib /tmp/jdd
