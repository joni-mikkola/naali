#!/bin/bash
TIMESTAMP=`date '+%d%m%H%M'`
SYSROOT=""
SCRIPTS=meegoScripts
BNUMBER=$1
NAALIDIR=/root/naali
WORKDIR=$(pwd)
mkdir -p log

#UPDATE SOURCES.LIST FOR REPOSITORY
cat > sources.list << EOF
deb http://repo.meego.com/MeeGo/sdk/host/repos/ubuntu/10.04/ /
deb http://repo.meego.com/MeeGo/tools/repos/ubuntu/10.04/ /
EOF

sudo cp sources.list /etc/apt/sources.list.d/

gpg --keyserver pgpkeys.mit.edu --recv 0BC7BEC479FC1F8A
gpg --export --armor 0BC7BEC479FC1F8A | sudo apt-key add -

sudo apt-get update
sudo apt-get -y install meego-sdk mic2
sudo mad-admin create -f meego-handset-ia32-1.1.2
sudo mad-admin set meego-handset-ia32-1.1.2
SYSROOT=`mad-admin query sysroot-dir`

sudo rm -fr $SYSROOT/$NAALIDIR

sudo git clone ../../. $SYSROOT/$NAALIDIR
cd $SYSROOT/$NAALIDIR
sudo git remote add -f upstream git://github.com/realXtend/naali.git
sudo git checkout tundra

cd $WORKDIR
sudo cp -r $SCRIPTS $SYSROOT/
sudo mic-chroot $SYSROOT -e "su -c ./meegoScripts/chrootbuild" 2>&1 | sudo tee ./log/$TIMESTAMP.log

VER=`sudo grep "Tundra" $SYSROOT/root/naali/Viewer/main.cpp | cut -d 'v' -f2 -|cut -d '-' -f 1`
BNUMBER=0

sudo mv $SYSROOT/packages/rpms/Tundra-Meego-0.0-1.i586.rpm $SYSROOT/packages/rpms/Tundra-Meego-$VER-$BNUMBER.i586-$TIMESTAMP.rpm
sudo mv $SYSROOT/packages/rpms/Tundra-Meego-Scenes-0.0-1.i586.rpm $SYSROOT/packages/rpms/Tundra-Meego-Scenes-$VER-$BNUMBER.i586-$TIMESTAMP.rpm
sudo cp $SYSROOT/packages/rpms/Tundra-Meego-Scenes-$VER-$BNUMBER.i586-$TIMESTAMP.rpm .
sudo cp $SYSROOT/packages/rpms/Tundra-Meego-$VER-$BNUMBER.i586-$TIMESTAMP.rpm . && echo "Tundra for MeeGo"

