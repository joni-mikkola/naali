#!/bin/bash

#DEFAULT VARIABLES
INSTALL_DIR=$PWD/lucid3
REX_DIR=realXtend
FINAL_INSTALL_DIR=/opt/realxtend
ARCH=amd64
LINUX_RELEASE=lucid
TAG=none
BUILDNUMBER=0
#IN CASE ERROR HAPPENS, $?-VARIABLE IS != 0
function errorCheck {
    if [ $? != 0 ];
    then
        echo "error" $1
	rm $INSTALL_DIR/proc
        exit 1
    fi
}

USAGE="\nUsage: $0 [--help] [-i install directory] [-b branch] [-t tag] [-a architecture] [-l linux release] 
		\nBranch is mandatory, select naali or tundra      	
		\nDefault settings 
      	\n   Install directory: $INSTALL_DIR
      	\n   Branch options: no default
		\n   Tag options: no default
      	\n   Architecture: $ARCH (i386/amd64)
      	\n   Linux release: $LINUX_RELEASE (lucid/maverick/natty)"

# Parse command line options.
while [ $# -gt 0 ]; do
    case "$1" in
    --help)
        echo -e $USAGE
        exit 0
        ;;
	-i)
	    shift
        if [ -z $1 ]; then
	        echo "-i option given, but no param for it"
		    exit 1
	    fi
	    INSTALL_DIR=$1
            echo $INSTALL_DIR
        echo "Branch option: $INSTALL_DIR"
	    shift;
        ;;
    -b)
	    shift
        if [ -z $1 ]; then
	        echo "-b option given, but no param for it"
		    exit 1
	    fi
	    BRANCH=$1
            echo $BRANCH
        echo "Branch option: $BRANCH"
	    shift;
        ;;
	-n)
	    shift
        if [ -z $1 ]; then
	        echo "-b option given, but no param for it"
		    exit 1
	    fi
	    BUILDNUMBER=$1
            echo $BRANCH
        echo "Buildnumber option: $BUILDNUMBER"
	    shift;
        ;;
	-t)
	    shift
        if [ -z $1 ]; then
	        echo "-t option given, but no param for it"
		    exit 1
	    fi
	    TAG=$1
            echo $BRANCH
        echo "Tag option: $TAG"
	    shift;
        ;;
	-a)
	    shift
        if [ -z $1 ]; then
		    echo "-a option given, but no param for it"
		    exit 1
	    fi
	    ARCH=$1
        echo "Architecture option: $ARCH"
	    shift;
        ;;
	-l)
	    shift
        if [ -z $1 ]; then
		    echo "-l option given, but no param for it"
		    exit 1
	    fi
	    LINUX_RELEASE=$1
        echo "Linux option: $LINUX_RELEASE"
	    shift;
        ;;
    *)
        # getopts issues an error message
        echo "Unknown param $1"
        exit 1
        ;;
    esac
done

if [ -z "$BRANCH" ];
then
	echo "No branch set"
	exit 1
fi

set -e
set -x

rm -fr $INSTALL_DIR

#CREATE FOLDER FOR DEBOOTSTRAP AND DOWNLOAD IT
#apt-get -y install debootstrap git-core fakeroot fakechroot

if [ ! -f $ARCH-$LINUX_RELEASE.tar ];
then
	fakeroot -s fakechroot.save fakechroot debootstrap --variant=fakechroot --arch $ARCH --make-tarball=$ARCH-$LINUX_RELEASE.tar $LINUX_RELEASE $INSTALL_DIR
fi
	fakeroot -i fakechroot.save fakechroot debootstrap --variant=fakechroot --arch $ARCH --unpack-tarball=$(pwd)/$ARCH-$LINUX_RELEASE.tar $LINUX_RELEASE $INSTALL_DIR

mkdir -p $INSTALL_DIR/$REX_DIR log
rm -fr $INSTALL_DIR/proc
ln -s /proc $INSTALL_DIR/

#CREATE LOCAL COPY OF NAALI.GIT
if [ ! -d $INSTALL_DIR/$REX_DIR/naali ];
then
	git clone ../../. $INSTALL_DIR/$REX_DIR/naali
fi

chmod 755 $INSTALL_DIR $INSTALL_DIR/$REX_DIR $INSTALL_DIR/$REX_DIR/naali
cd $INSTALL_DIR/$REX_DIR/naali
git remote add -f upstream git://github.com/realXtend/naali.git
git checkout $BRANCH


if [ $TAG != "none" ];
then
	git show-ref $TAG
	if [ $? -ne 0 ];
	then
		echo "Invalid tag" $TAG
		exit 1
	fi

	git checkout $TAG
	VER=$TAG	
else
	if [ $BRANCH == "tundra" ];
	then
		VER=`grep "Tundra" $INSTALL_DIR/$REX_DIR/naali/Viewer/main.cpp | cut -d 'v' -f2 -|cut -d '-' -f 1`
	else
		VER=`grep "Naali_v" Application/main.cpp | cut -d 'v' -f2 | tail -1 |cut -d '"' -f1`
		BRANCH=naali
	fi
fi

cd ../../../

#MOVE SOME CACHED DEBS FROM BACKUP TO REDUCE BUILD TIME
if [ -d ./apt_cache_$ARCH/ ];
then
	if [ -f $INSTALL_DIR/var/cache/apt/archives/*.deb ];
	then	
		rm $INSTALL_DIR/var/cache/apt/archives/*.deb
	fi
	chmod -R 755 ./apt_cache_$ARCH
	mkdir -p $INSTALL_DIR/var/cache/apt/archives/
	cp -r  ./apt_cache_$ARCH/*.deb $INSTALL_DIR/var/cache/apt/archives/
fi

chmod -R a+rX $INSTALL_DIR/$REX_DIR/
chmod 755 ./config/chroot-script.bash
rm -fr $INSTALL_DIR/$REX_DIR/config
cp ./config/build-ubuntu-deps.bash $INSTALL_DIR/$REX_DIR/naali/tools/	
cp -r ./config $INSTALL_DIR/$REX_DIR/config

#CHROOT INTO OUR UBUNTU AND RUN SCRIPT (PARAMETERS BRANCH + VERSION) + DO LOG FILE
LOGFILE=` date|awk 'OFS="."{print $2,$3,$6,$4}'`
fakeroot -i fakechroot.save fakechroot chroot $INSTALL_DIR $REX_DIR/config/chroot-script.bash $BRANCH $ARCH $REX_DIR $TAG $BUILDNUMBER $VER 2>&1 | tee ./log/$LOGFILE-$BRANCH-$ARCH.log 

if [ ! -d ./apt_cache_$ARCH/ ];
then
	mkdir -p ./apt_cache_$ARCH/
	cp -r $INSTALL_DIR/var/cache/apt/archives/*.deb ./apt_cache_$ARCH/
fi

#MOVE DEB FILES BACK TO OUR CURRENT DIRECTORY
chmod -R a+rX $INSTALL_DIR/$REX_DIR/
mv -f $INSTALL_DIR/$REX_DIR/*.deb ./

rm $INSTALL_DIR/proc

