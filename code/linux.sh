#!/bin/sh

if [[ ! -d "../build" ]];
then
    mkdir "../build"
fi

pushd "../build" > /dev/null

compiler_options="-O0 -g -ggdb -Iinclude"
linker_options="-lm"

# build ludum.so
#
gcc $compiler_options -shared -fPIC "../code/ludum.c" -o "ludum.so" $linker_options

# build ludum (executable)
#
gcc $compiler_options "../code/ludum_main.c" -o "ludum" $linker_options

popd > /dev/null
