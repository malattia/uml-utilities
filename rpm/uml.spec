%define version 0.37-2.4.0
Name:				User-mode Linux
Summary:			The usermode port of Linux
Version:			%{version}
Release:			0
Copyright:			GPL
Packager:			Jeff Dike <jdike@karaya.com>
Group:				Utilities/System
Vendor:				Jeff Dike <jdike@karaya.com>
URL:				http://user-mode-linux.sourceforge.net/
BuildRoot:			/tmp/uml-broot


%description
User-mode Linux is the usermode port of the Linux kernel.  This package
These tools allow you to manage the Ethernet driver which works with
User-Mode Linux (UML).


%changelog
* Sat Aug 12 2000 William Stearns <wstearns@pobox.com>
- first test release with early tools.


%prep
%setup -c uml-net-tools


%build
make all


%install
if [ "$RPM_BUILD_ROOT" = "/tmp/uml-net-tools-broot" ]; then
    rm -rf $RPM_BUILD_ROOT

	install -d $RPM_BUILD_ROOT/sbin
	cp -p um_eth_net_util $RPM_BUILD_ROOT/sbin
	cp -p um_eth_net_set $RPM_BUILD_ROOT/sbin
else
	echo Invalid Build root
	exit 1
fi

						
%clean
if [ "$RPM_BUILD_ROOT" = "/tmp/uml-net-tools-broot" ]; then
	rm -rf $RPM_BUILD_ROOT
else
	echo Invalid Build root
	exit 1
fi


%files
%defattr(-,root,root)
%attr(755,root,root)				/sbin/um_eth_net_util
%attr(755,root,root)				/sbin/um_eth_net_set
							%doc	README
