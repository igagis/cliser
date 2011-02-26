#!/bin/sh

soName=1

packageName=libcliser0-dbg

baseDir=debian/out/$packageName
mkdir -p $baseDir

#copy files
libDir=$baseDir/usr/lib/debug/usr/lib
mkdir -p $libDir

libFileName=libcliser.so

cp src/$libFileName.$soName $libDir



#create dir where the output 'control' will be placed
mkdir -p $baseDir/DEBIAN

#remove substvars
rm -f debian/substvars

#calculate dependancies
dpkg-shlibdeps $libDir/$libFileName.$soName

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir ../file.deb
dpkg-name ../file.deb #rename file to proper debian format (package_version_arch.deb)
