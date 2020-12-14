#!/bin/sh
git clone --recursive https://github.com/igrr/mkspiffs.git ./util/mkspiffs/
cd ./util/mkspiffs/
git submodule update --init --recursive;
make dist CPPFLAGS="-DSPIFFS_ALIGNED_OBJECT_INDEX_TABLES=1 -DSPIFFS_OBJ_META_LEN=0"
cd ../../
