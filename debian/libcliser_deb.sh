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

#=======================================
#=====Package name======================
#=======================================
#TODO: get version
packageFileName=${packageName}_0.0.1-1_$(dpkg-architecture -qDEB_BUILD_ARCH).deb


#create dir where the output 'control' will be placed
mkdir -p $baseDir/DEBIAN

#calculate dependancies
dpkg-shlibdeps $libDir/$libFileName.$soName

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir ../$packageFileName
