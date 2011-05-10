Summary: Freeimage
Name: freeimage
Version: 1.0
Release: meego
License: GPL
Group: System Environment/Base
Source: http://sourceforge.net/projects/FreeImage3141.zip
BuildRoot: /home/meego/rpmbuild/BUILDROOT/%{name}-%{version}-%{release}-root


%description
FreeImage description

%prep
%setup -q -n FreeImage

%build
make -j `grep -c "^processor" /proc/cpuinfo`

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
make clean


%files
	%defattr(-,root,root)
	%doc
	/usr/src/*
	/usr/include/*
	/usr/lib/*


%changelog

