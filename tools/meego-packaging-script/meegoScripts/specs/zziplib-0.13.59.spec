Summary: zziplib
Name: zziplib
Version: 0.13.59
Release: meego
License: GPL
Group: System Environment/Base
Source: http://sourceforge.net/projects/zziplib-0.13.59.tar.bz2
BuildRoot: /home/meego/rpmbuild/BUILDROOT/%{name}-%{version}-%{release}-root


%description
zziplib description

%prep
%setup -q -n %{name}-%{version}

%build
./configure --prefix=/usr
make  -j `grep -c "^processor" /proc/cpuinfo`

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
make clean


%files
	%defattr(-,root,root)
	/usr/include/*
	/usr/lib/*
	/usr/bin/*
	/usr/share/*


%changelog

