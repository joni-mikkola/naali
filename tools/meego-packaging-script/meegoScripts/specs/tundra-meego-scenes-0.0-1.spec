Summary: Tundra viewer scenes
Name: Tundra-Meego-Scenes
Version: 0.0
Release: 1
License: GPL
Group: Applications/Internet
Source: naali-latest.tar.gz
Patch: Tundra-Meego.patch
BuildRoot: /opt/realXtend/

%description
Tundra is software to view and host interconnected 3D worlds or "3D internet".

%prep

%build

%install
mkdir -p /root/rpmbuild/BUILDROOT/Tundra-Meego-Scenes-0.0-1.i386/opt/realXtend/
cp -r /root/naali/bin/scenes /root/rpmbuild/BUILDROOT/Tundra-Meego-Scenes-0.0-1.i386/opt/realXtend/

%clean
#rm -rf $RPM_BUILD_ROOT

%files
/opt/realXtend/*
%defattr(-,root,root)
%dir /opt/realXtend/

%doc
/opt/realXtend/*

%changelog


