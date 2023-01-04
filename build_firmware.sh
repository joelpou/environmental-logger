#!/bin/sh
export VERSION_STRING="$(cat src/ra-rel.ino  | grep -oE "(v.....-..)")"
echo " Version String:|$VERSION_STRING|"  

export ARGON_OUTPUT=rel-argon-$VERSION_STRING.bin

export XENON_OUTPUT=rel-xenon-$VERSION_STRING.bin
echo "Compiling Argon Firmware"
particle compile -v argon --saveTo  $ARGON_OUTPUT || { echo "Particle Argon Build Failed"; exit 1;}
echo "Compiling Xenon Firmware"
particle compile -v xenon --saveTo $XENON_OUTPUT || { echo "Particle Xenon Build Failed"; exit 1;}

echo "Argon bin MD5"
md5sum $ARGON_OUTPUT > $ARGON_OUTPUT.md5 || { echo "Error getting argon .bin MD5"; exit 1;}
echo "Xenon bin MD5"
md5sum $XENON_OUTPUT > $XENON_OUTPUT.md5|| { echo "Error getting xenon .bin md5"; exit 1;}

git status
read -n1 -r -p "Add the files that you changed to increase the version to git and commit them now!!" 