%define version 0.43_2.4.6

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
This is the user-mode port of the Linux kernel - it runs a virtual 
machine in a set of Linux processes.  This package also contains a 
number of other useful tools and utilities related to UML.

%changelog
* Wed Jul  4 2001 Jeff Dike <jdike@karaya.com>
- Merged the management console
- Many bug fixes
- 64 bit IO works again

* Wed Mar  7 2001 Jeff Dike <jdike@karaya.com>
- Redid the network drivers

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

    make -C $CVS_POOL/doc/web UserModeLinux-HOWTO.txt
    make -C $CVS_POOL/tools/uml_net all
    make -C $CVS_POOL/tools/uml_router all
    make -C $CVS_POOL/tools/mconsole all
else
	echo Invalid Build root
	exit 1
fi

%install
if [ "$RPM_BUILD_ROOT" = "/tmp/uml-kit" ]; then

    install -d $RPM_BUILD_ROOT/usr/bin
    install -s $RPM_BUILD_ROOT/usr/src/linux/linux $RPM_BUILD_ROOT/usr/bin

    cd $RPM_BUILD_ROOT/usr/src/linux
    mkdir -p $RPM_BUILD_ROOT/lib/modules/`./linux --version`
    ln -s $RPM_BUILD_ROOT/lib/modules/`./linux --version` \
	$RPM_BUILD_ROOT/lib/modules/`./linux --version | sed s/-.um//`
    make modules_install ARCH=um INSTALL_MOD_PATH=$RPM_BUILD_ROOT
    rm -f $RPM_BUILD_ROOT/lib/modules/`./linux --version | sed s/-.um//`
    cd $RPM_BUILD_ROOT
    tar cf $RPM_BUILD_ROOT/modules.tar lib
    gzip $RPM_BUILD_ROOT/modules.tar
    install -d $RPM_BUILD_ROOT/usr/lib/uml
    install $RPM_BUILD_ROOT/modules.tar.gz $RPM_BUILD_ROOT/usr/lib/uml

    install $CVS_POOL/tools/redhat/mkrootfs $RPM_BUILD_ROOT/usr/bin/mkrootfs
    install $CVS_POOL/tools/redhat/functions \
	$RPM_BUILD_ROOT/usr/lib/uml/functions
    install $CVS_POOL/tools/uml_net/uml_net $RPM_BUILD_ROOT/usr/bin/uml_net
    install $CVS_POOL/tools/uml_router/uml_router \
	$RPM_BUILD_ROOT/usr/bin/uml_router
    install $CVS_POOL/tools/mconsole/uml_mconsole \
	$RPM_BUILD_ROOT/usr/bin/uml_mconsole

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
%attr(755,root,root)				/usr/lib/uml/functions
%attr(4755,root,root)				/usr/bin/uml_net
%attr(755,root,root)				/usr/bin/uml_router
%attr(755,root,root)				/usr/bin/uml_mconsole
%attr(644,root,root)                            /usr/doc/HOWTO/UserModeLinux-HOWTO
