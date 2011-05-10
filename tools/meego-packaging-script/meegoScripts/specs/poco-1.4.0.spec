Summary: poco
Name: poco
Version: 1.4.0
Release: meego
License: GPL
Group: System Environment/Base
Source: http://sourceforge.net/projects/xmlrpc-epi/files/xmlrpc-epi-base/0.54.1/poco-1.4.0.tar.gz
BuildRoot: /home/juha/rpmbuild/BUILDROOT/%{name}-%{version}-%{release}-root
URL: http://poco.com

%description
poco

%prep
%setup -q -n poco-1.4.0

%build
./configure --omit=Data/ODBC,Data/MySQL --prefix=/usr
gmake  -j `grep -c "^processor" /proc/cpuinfo`

%install
rm -rf %{buildroot}
gmake install DESTDIR=%{buildroot}




%files
	%defattr(-,root,root)
	%doc
	/usr/include/*
	/usr/lib/*


%changelog

