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

#=======================================
#=====Package name======================
#=======================================
#TODO: get arch
packageFileName=${packageName}_0.0.1-1_i386.deb


#create dir where the output 'control' will be placed
mkdir -p $baseDir/DEBIAN

#calculate dependancies
dpkg-shlibdeps $libDir/$libFileName.$soName

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir ../$packageFileName
