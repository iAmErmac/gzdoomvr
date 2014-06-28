#!/bin/sh

mkdir -p build
cd build


if [ "$(uname -m)" = "x86_64" ]; then 
    lib=libfmodex64
else
    lib=libfmodex
fi

# get latest fmod ex version
changelog=revision_4.44.txt
wget http://www.fmod.org/files/$changelog

pointversion=$(grep -e "Stable branch update" $changelog | cut -d' ' -f2 | head -n1)
version=$(echo $pointversion | sed -e 's/\.//g')

dirname=fmodapi${version}linux
fname=${dirname}.tar.gz

wget "http://www.fmod.org/download/fmodex/api/Linux/$fname"
tar xvf "$fname"

# FMOD ex doesn't have a soname entry. The linker will therefore use
# its filename to declare dependencies. Using a different filename allows
# manual library updates without re-linking the GZ3Doom binary.
mv $dirname/api/lib/$lib-$pointversion.so libfmodex-4.44.so




# Install dependencies:
# sudo apt-get install chrpath cmake libfluidsynth-dev libglew-dev libgtk2.0-dev libsdl1.2-dev

cmake \
-DCMAKE_INSTALL_PREFIX=/usr \
-DCMAKE_BUILD_TYPE=Release \
-DFORCE_INTERNAL_BZIP2=ON \
-DFORCE_INTERNAL_GME=ON \
-DFORCE_INTERNAL_JPEG=ON \
-DFORCE_INTERNAL_ZLIB=ON \
-DFMOD_LIBRARY=libfmodex-4.44.so \
-DFMOD_INCLUDE_DIR=$dirname/api/inc ..

make
chrpath -cr '$ORIGIN' GZ3Doom

target=GZ3Doom-linux-$(uname -m)
mkdir -p $target
cp *.pk3 *.so GZ3Doom $target
strip $target/liboutput_sdl.so
chmod 0644 $target/liboutput_sdl.so

tar cvf - $target | xz > ../installer/linux/$target.tar.xz

echo ""
echo "'installer/linux/$target.tar.xz' created."

