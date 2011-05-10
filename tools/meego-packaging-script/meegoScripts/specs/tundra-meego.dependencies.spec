Summary: Tundra dependencies
Name: Tundra-Dependencies
Version: 0.0
Release: 1
Group: Applications/Internet
Source: tundra-meego-dependencies.tar.gz
License: GPL 
Patch: Tundra-Meego.patch
BuildRoot: /opt/tundra/

%description
Tundra is software to view and host interconnected 3D worlds or "3D internet".

%prep

%build

%install
cp -r /root/rpmbuild/BUILD/bin BUILDROOT/Tundra-Meego-0.0-1.i586/

%clean

%files
/bin/ 

%changelog


