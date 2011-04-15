#!/bin/bash

export LANG=C

FINAL_INSTALL_DIR=/opt/realxtend
PACKAGE_NAME=realXtend
DATE=`date '+%y%m%d'`
TIME=`date '+%H%M'`

#PARAMETERS PASSED 
BRANCH=$1
ARCH=$2
REX_DIR=$3
TAG=$4
BUILDNUMBER=$5
VER=$6
#IN CASE ERROR HAPPENS, $?-VARIABLE IS != 0
function errorCheck {
    if [ $? -ne 0 ];
    then
        echo $1
        exit $?
    fi
}

#UPDATE SOURCES.LIST FOR REPOSITORY
cat > /etc/apt/sources.list << EOF
deb http://se.archive.ubuntu.com/ubuntu/ lucid main restricted
deb-src http://se.archive.ubuntu.com/ubuntu/ lucid main restricted

deb http://se.archive.ubuntu.com/ubuntu/ lucid-updates main restricted
deb-src http://se.archive.ubuntu.com/ubuntu/ lucid-updates main restricted

deb http://se.archive.ubuntu.com/ubuntu/ lucid universe
deb-src http://se.archive.ubuntu.com/ubuntu/ lucid universe
deb http://se.archive.ubuntu.com/ubuntu/ lucid-updates universe
deb-src http://se.archive.ubuntu.com/ubuntu/ lucid-updates universe

deb http://se.archive.ubuntu.com/ubuntu/ lucid multiverse
deb-src http://se.archive.ubuntu.com/ubuntu/ lucid multiverse
deb http://se.archive.ubuntu.com/ubuntu/ lucid-updates multiverse
deb-src http://se.archive.ubuntu.com/ubuntu/ lucid-updates multiverse

deb http://security.ubuntu.com/ubuntu lucid-security main restricted
deb-src http://security.ubuntu.com/ubuntu lucid-security main restricted
deb http://security.ubuntu.com/ubuntu lucid-security universe
deb-src http://security.ubuntu.com/ubuntu lucid-security universe
deb http://security.ubuntu.com/ubuntu lucid-security multiverse
deb-src http://security.ubuntu.com/ubuntu lucid-security multiverse
EOF

dhclient

#UPDATE REPOSITORY
apt-get update
apt-get -y install git-core wget unzip
dpkg --force-all -i /var/cache/apt/archives/*.deb

cd /$REX_DIR/naali

rm -fr /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH
rm -fr /$REX_DIR/$PACKAGE_NAME-$BRANCH-scenes-noarch

#UPDATE CORRECT VERSION NUMBERS + ARCHITECTURE
sed '/Architecture/c \Architecture: '$ARCH'' /$REX_DIR/config/control_$BRANCH > tmpfile ; mv tmpfile /$REX_DIR/config/control_$BRANCH
sed '/Version/c \Version: '$VER'' /$REX_DIR/config/control_$BRANCH > tmpfile ; mv tmpfile /$REX_DIR/config/control_$BRANCH

#CREATE STRUCTURE NEEDED FOR DEB PACKAGE && USE READY-MADE CONTROL FILE && DESKTOP ICON
mkdir -p /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/DEBIAN
mkdir -p /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH/

if [ $BRANCH == "tundra" ];
then
	#CREATE STRUCTURE FOR SCENES PACKAGE
	sed '/Version/c \Version: '$VER'' /$REX_DIR/config/control_scenes > tmpfile ; mv tmpfile /$REX_DIR/config/control_scenes
	mkdir -p /$REX_DIR/$PACKAGE_NAME-$BRANCH-scenes-noarch/DEBIAN
	mkdir -p /$REX_DIR/$PACKAGE_NAME-$BRANCH-scenes-noarch/$FINAL_INSTALL_DIR-$BRANCH/
	cp /$REX_DIR/config/control_scenes /$REX_DIR/$PACKAGE_NAME-$BRANCH-scenes-noarch/DEBIAN/control

	cp /$REX_DIR/config/run-linux_${BRANCH}_server.sh /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH/run-server.sh
fi

cp -r /$REX_DIR/config/usr_$BRANCH /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/usr

cp /$REX_DIR/config/run-linux_${BRANCH}_viewer.sh /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH/run-viewer.sh
cp /$REX_DIR/config/control_$BRANCH /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/DEBIAN/control

#WE NEED TO REMOVE -g SWITCH FROM THE BASH SCRIPT BELOW SINCE WE DON'T NEED DEBUG INFORMATION + FILE SIZE INCREASES DRAMATICALLY
sed -i 's/ccache g++ -O -g/ccache g++ -O/g' /$REX_DIR/naali/tools/build-ubuntu-deps.bash

/$REX_DIR/naali/tools/build-ubuntu-deps.bash
errorCheck "Check for error with build process"

cp -r /$REX_DIR/naali/bin/* /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH

#FOLLOWING LINES WILL BE REMOVED LATER ON
cp /$REX_DIR/naali-deps/build/PythonQt2.0.1/lib/libPythonQt_QtAll.so.1 /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH
cp /$REX_DIR/naali-deps/build/PythonQt2.0.1/lib/libPythonQt.so.1 /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH/
cp /$REX_DIR/naali-deps/build/qtpropertybrowser-2.5_1-opensource/lib/libQtSolutions_PropertyBrowser-2.5.so.1 /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH
#########################################

#CREATE DEB PACKAGE USING DPKG, BY DEFAULT THE PACKAGE NAME WILL BE THE FOLDER NAME
cd /$REX_DIR

if [ $BRANCH == "tundra" ];
then
		
	mv $PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH/scenes /$REX_DIR/$PACKAGE_NAME-$BRANCH-scenes-noarch/$FINAL_INSTALL_DIR-$BRANCH/
	chmod -R a+rX $PACKAGE_NAME-$BRANCH-scenes-noarch
	dpkg -b $PACKAGE_NAME-$BRANCH-scenes-noarch
fi

cp /$REX_DIR/naali-deps/build/knet/lib/libkNet.so /$REX_DIR/$PACKAGE_NAME-$BRANCH-$VER-$ARCH/$FINAL_INSTALL_DIR-$BRANCH

chmod -R a+rX $PACKAGE_NAME-$BRANCH-$VER-$ARCH
dpkg -b  $PACKAGE_NAME-$BRANCH-$VER-$ARCH
errorCheck "Check for error with dpkg"
	
mv $PACKAGE_NAME-$BRANCH-$VER-$ARCH.deb $PACKAGE_NAME-$BRANCH-$VER-$DATE-$BUILDNUMBER-$ARCH.deb
