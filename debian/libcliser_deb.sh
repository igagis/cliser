#!/bin/sh

soName=1

packageName=libcliser0

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

#calculate dependancies
dpkg-shlibdeps $libDir/$libFileName.$soName

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir ../file.deb
dpkg-name ../file.deb #rename file to proper debian format (package_version_arch.deb)
