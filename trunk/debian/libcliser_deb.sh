#!/bin/sh

soName=0

packageName=libcliser$soName

baseDir=debian/out/$packageName
mkdir -p $baseDir

#copy files
libDir=$baseDir/usr/lib
mkdir -p $libDir

libFileName=libcliser.so

cp src/$libFileName.$soName $libDir
strip -g $libDir/$libFileName.$soName
ln -s /usr/lib/$libFileName.$soName $libDir/$libFileName



#create dir where the output 'control' will be placed
mkdir -p $baseDir/DEBIAN

#remove substvars
rm -f debian/substvars

#calculate dependancies
dpkg-shlibdeps $libDir/$libFileName.$soName

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir tmp-package.deb
dpkg-name -o -s .. tmp-package.deb #rename file to proper debian format (package_version_arch.deb)
