#!/bin/sh

packageName=libcliser-dev

baseDir=debian/out/$packageName
mkdir -p $baseDir

#copy files
incDir=$baseDir/usr/include/cliser
mkdir -p $incDir

cp src/cliser/*.hpp $incDir

#=======================================
#=====Package name======================
#=======================================
#TODO: get arch
packageFileName=${packageName}_0.0.1-1_i386.deb


#create dir where the output 'control' will be placed
mkdir -p $baseDir/DEBIAN

#remove substvars
rm debian/substvars

#generate final control file
dpkg-gencontrol -p$packageName -P$baseDir

dpkg -b $baseDir ../$packageFileName
