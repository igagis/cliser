#!/bin/sh

packageName=libcliser-dev

soName=0

libFileName=libcliser.so

baseDir=debian/out/$packageName
mkdir -p $baseDir

#copy files
incDir=$baseDir/usr/include/cliser
mkdir -p $incDir

cp src/cliser/*.hpp $incDir


libDir=$baseDir/usr/lib
mkdir -p $libDir

ln -s /usr/lib/$libFileName.$soName $libDir/$libFileName



#create dir where the output 'control' will be placed
mkdir -p $baseDir/DEBIAN

#remove substvars
rm -f debian/substvars

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir tmp-package.deb
dpkg-name -o -s .. tmp-package.deb #rename file to proper debian format (package_version_arch.deb)
