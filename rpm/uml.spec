%define version 0.39_2.4.2
%define kernel_pool um

Name:				user-mode-linux
Summary:			The user-mode port of the Linux kernel
Version:			%{version}
Release:			0
Copyright:			GPL
Packager:			Jeff Dike <jdike@karaya.com>
Group:				Utilities/System
Source:				http://download.sourceforge.net/user-mode-linux/uml-patch-%{version}
Vendor:				Jeff Dike <jdike@karaya.com>
URL:				http://user-mode-linux.sourceforge.net/
BuildRoot:			/tmp/uml-kit


%description
This is the user-mode port of the Linux kernel - it runs a virtual machine in 
a set of Linux processes.  This package also contains a number of other useful
tools and utilities related to UML.

%changelog
* Wed Mar  7 2001 Jeff Dike <jdike@karaya.com>
- initial version


%prep

if [ "$CVS_POOL" = "" ]; then
    echo CVS_POOL not defined
    exit 1
fi

if [ "$KERNEL_POOL" = "" ]; then
    echo KERNEL_POOL not defined
    exit 1
fi

if [ "$RPM_BUILD_ROOT" = "/tmp/uml-kit" ]; then
    rm -rf $RPM_BUILD_ROOT
    mkdir $RPM_BUILD_ROOT

    mkdir -p $RPM_BUILD_ROOT/usr/src/
    cp -al $KERNEL_POOL $RPM_BUILD_ROOT/usr/src/linux
    cp -alf $CVS_POOL/linux $RPM_BUILD_ROOT/usr/src/
    cd $RPM_BUILD_ROOT/usr/src/linux
    make mrproper ARCH=um

else
	echo Invalid Build root
	exit 1
fi

%build

if [ "$CVS_POOL" = "" ]; then
    echo CVS_POOL not defined
    exit 1
fi

if [ "$KERNEL_POOL" = "" ]; then
    echo KERNEL_POOL not defined
    exit 1
fi

if [ "$RPM_BUILD_ROOT" = "/tmp/uml-kit" ]; then
    cd $RPM_BUILD_ROOT/usr/src/linux
    cp arch/um/config.release arch/um/defconfig
    make oldconfig ARCH=um
    make linux ARCH=um
    make modules ARCH=um
    make modules_install INSTALL_MOD_PATH=$RPM_BUILD_ROOT/lib/modules/`./linux --version`

    make -C $CVS_POOL/doc/web UserModeLinux-HOWTO.txt
    make -C $CVS_POOL/tools/umn all
    make -C $CVS_POOL/tools/net all
else
	echo Invalid Build root
	exit 1
fi

%install
if [ "$RPM_BUILD_ROOT" = "/tmp/uml-kit" ]; then

    install -d $RPM_BUILD_ROOT/usr/bin
    install -s $RPM_BUILD_ROOT/usr/src/linux/linux $RPM_BUILD_ROOT/usr/bin

    cd $RPM_BUILD_ROOT/usr/src/linux
    make modules_install INSTALL_MOD_PATH=modules
    cd modules
    tar cf $RPM_BUILD_ROOT/modules.tar .
    gzip $RPM_BUILD_ROOT/modules.tar
    install -d $RPM_BUILD_ROOT/usr/lib/uml
    install $RPM_BUILD_ROOT/modules.tar.gz $RPM_BUILD_ROOT/usr/lib/uml

    install $CVS_POOL/tools/redhat/mkrootfs $RPM_BUILD_ROOT/usr/bin/mkrootfs
    install $CVS_POOL/tools/umn/umn_helper $RPM_BUILD_ROOT/usr/bin/umn_helper
    install $CVS_POOL/tools/net/um_eth_serv $RPM_BUILD_ROOT/usr/bin/um_eth_serv
    install $CVS_POOL/tools/net/um_eth_tool $RPM_BUILD_ROOT/usr/bin/um_eth_tool
    
    install -d $RPM_BUILD_ROOT/usr/doc/HOWTO
    install $CVS_POOL/doc/web/UserModeLinux-HOWTO.txt $RPM_BUILD_ROOT/usr/doc/HOWTO/UserModeLinux-HOWTO

else
	echo Invalid Build root
	exit 1
fi

						
%clean
if [ "$RPM_BUILD_ROOT" = "/tmp/uml-kit" ]; then
	rm -rf $RPM_BUILD_ROOT
else
	echo Invalid Build root
	exit 1
fi


%files
%defattr(-,root,root)
%attr(755,root,root)				/usr/bin/linux
%attr(644,root,root)                            /usr/lib/uml/modules.tar.gz
%attr(755,root,root)				/usr/bin/mkrootfs
%attr(4755,root,root)				/usr/bin/umn_helper
%attr(755,root,root)				/usr/bin/um_eth_serv
%attr(755,root,root)				/usr/bin/um_eth_tool
%attr(644,root,root)                            /usr/doc/HOWTO/UserModeLinux-HOWTO
