Summary: XMLRPC-epi
Name: xmlrpc-epi
Version: 0.54.1
Release: meego
License: GPL
Group: System Environment/Base
Source: http://sourceforge.net/projects/xmlrpc-epi/files/xmlrpc-epi-base/0.54.1/xmlrpc-epi-0.54.1.tar.gz
BuildRoot: /home/meego/rpmbuild/BUILDROOT/%{name}-%{version}-%{release}-root
URL: http://sourceforge.net/projects/xmlrpc-epi/

%description
xmlrpc-epi description

%prep
%setup -q -n xmlrpc

%build
./configure
make  -j `grep -c "^processor" /proc/cpuinfo`

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}




%files
	%defattr(-,root,root)
	%doc
	/usr/local/bin/*
	/usr/local/include/*
	/usr/local/lib/*


%changelog

