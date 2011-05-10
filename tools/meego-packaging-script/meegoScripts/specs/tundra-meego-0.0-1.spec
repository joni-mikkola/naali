Summary: Tundra viewer
Name: Tundra-Meego
Version: 0.0
Release: 1
License: GPL
Group: Applications/Internet
BuildRoot: /opt/realXtend/
Requires: libqtscripttools4, libphonon4
%description
Tundra is software to view and host interconnected 3D worlds or "3D internet".

%prep

%build

%install
mkdir -p /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib
cp -r /root/naali/bin/* /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/
rm -fr /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/scenes
cp -fr /root/libs/* /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib
cp /root/rpmbuild/SOURCES/openal-soft-1.13/build/libopenal.so.1 /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib/
cp /root/rpmbuild/SOURCES/knet/lib/libkNet.so /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib/
cp /root/rpmbuild/SOURCES/PythonQt2.0.1/lib/libPythonQt_QtAll.so.1 /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib/
cp /root/rpmbuild/SOURCES/PythonQt2.0.1/lib/libPythonQt.so.1 /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib/
cp /root/rpmbuild/SOURCES/qtpropertybrowser-2.5_1-opensource/lib/libQtSolutions_PropertyBrowser-2.5.so.1 /root/rpmbuild/BUILDROOT/Tundra-Meego-0.0-1.i386/opt/realXtend/lib/

%clean
#rm -rf $RPM_BUILD_ROOT

%files
/opt/realXtend/*
%defattr(-,root,root)
%dir /opt/realXtend/

%doc
/opt/realXtend/*

%changelog


