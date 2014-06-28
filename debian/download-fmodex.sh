#!/bin/sh

cd build

if [ "$(uname -m)" = "x86_64" ]; then 
    lib=libfmodex64
else
    lib=libfmodex
fi

changelog=revision_4.44.txt
wget http://www.fmod.org/files/$changelog

pointversion=$(grep -e "Stable branch update" $changelog | cut -d' ' -f2 | head -n1)
version=$(echo $pointversion | sed -e 's/\.//g')

dirname=fmodapi${version}linux
fname=${dirname}.tar.gz

wget "http://www.fmod.org/download/fmodex/api/Linux/$fname"

