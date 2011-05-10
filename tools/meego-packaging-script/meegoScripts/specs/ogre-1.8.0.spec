Summary: OGRE3D engine for 3D graphics
Name: ogre
Version: 1.8.0
Release: meego
##target_arch: i586
License: GPL
Group: System Environment/Base
Source: http://downloads.sourceforge.net/project/ogre/ogre/1.7/ogre-1.8.0.tar.gz
BuildRoot: /home/juha/rpmbuild/BUILDROOT/%{name}-%{version}-%{release}-root
URL: http://www.ogre3d.org
#ei me voida tarkistaa noita sysrootista? -- rpmbuild tukee jotain chrootjuttuja mutta..
#listattu ogre3d.org...
#BuildRequires: build-essential, automake, libtool
#BuildRequires: libfreetype6-dev, libfreeimage-dev, libzzip-dev, libxrandr-dev, libxaw7-dev
#BuildRequires: nvidia-cg-toolkit, libois-dev, libboost-thread-dev

Requires: xorg-x11-proto-xproto, xorg-x11-proto-kbproto, libpthread-stubs, xorg-x11-filesystem, xorg-x11-proto-randrproto, xorg-x11-proto-renderproto

%description
OGRE is a scene-oriented, flexible 3D engine written in C++ designed
to make it easier and more intuitive for developers to produce applications
utilising hardware-accelerated 3D graphics. The class library abstracts all
the details of using the underlying system libraries like Direct3D and OpenGL
and provides an interface based on world objects and other intuitive classes.

%package	devel
Summary:	Headers and libraries for building apps that use OGRE
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description 	devel
This package contains headers and libraries required to build applications that
use the OGRE3D engine.


%prep
%setup -q -n %{name}


%build
cmake -DCMAKE_INSTALL_PREFIX=/usr .
make  -j `grep -c "^processor" /proc/cpuinfo`

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}


%clean
rm -rf %{buildroot}

%files
	%defattr(-,root,root)
	%doc
	/usr/bin/*
	/usr/lib/*.so.*

%files devel
	%doc
	%defattr(-,root,root,-)
	/usr/include/OGRE/
	/usr/lib/*.so
	/usr/lib/OGRE/
	/usr/lib/pkgconfig/*.pc

%changelog

