#!/bin/sh

# Debian policy requires that symlinks *.so will come in -dev package
# but it hardly achievable with SCons, so we apply this small script
# after scons install

TSLOAD_LIBS=debian/tsload/usr/lib
DEV_LIBS=debian/tsload-dev/usr/lib

mkdir -p $DEV_LIBS

for LINK in libhostinfo.so libtscommon.so 	\
			libtsfile.so   libtsjson.so		\
			libtsload.so   libtsobj.so
do
	mv $TSLOAD_LIBS/$LINK $DEV_LIBS/$LINK
done
