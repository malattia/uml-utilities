#!/bin/bash
#These are functions useful to the uml projects.  They are GPL'd.
#Copyright 1999-2000, William Stearns <wstearns@pobox.com> and
#Jeff Dike <jdike@karaya.com>

addline() {
#Params: $1 File that needs the additional line, $2 line to add.
    if [ "$#" != "2" ]; then
	echo Incorrect number of arguments to addline! >/dev/stderr
    else
        case "$1" in
	/*)
	    echo Filename is not relative in addline! >/dev/stderr
	    ;;
	*)
	    if [ -f "$1" ] && $SUDO cat "$1" | grep -q "^$2\$" ; then
		echo \"$2\" is already in $1 - not adding again. >/dev/stderr
	    else
		printf "%-3s%-40s%-50s\n" '+' "$1" "$2"		#Was: echo Adding \"$2\" to $1
    		if [ -f $1 ]; then
    		    #Yes, this is ugly, but _you_ try getting sudo to allow you to append as root as well!
		    #$SUDO /bin/echo "$2" >>$1	- doesn't work.
		    $SUDO cp -pf "$1" "$1.orig"
		    echo "$2" | $SUDO /bin/cat "$1.orig" - | $SUDO dd of="$1" 2>/dev/null
		    $SUDO rm -f "$1.orig"
		else
		    $SUDO touch "$1"
		    echo "$2" | $SUDO dd of="$1" 2>/dev/null
		fi
	    fi
	    ;;
	esac
    fi
}

#-------------------------------------------------------------------------
# bits2mask function, returns the netmask for the number of bits parameter.
#-------------------------------------------------------------------------
bits2mask () {
	case $1 in
	32|*/32)	echo 255.255.255.255	;;
	31|*/31)	echo 255.255.255.254	;;
	30|*/30)	echo 255.255.255.252	;;
	29|*/29)	echo 255.255.255.248	;;
	28|*/28)	echo 255.255.255.240	;;
	27|*/27)	echo 255.255.255.224	;;
	26|*/26)	echo 255.255.255.192	;;
	25|*/25)	echo 255.255.255.128	;;

	24|*/24)	echo 255.255.255.0	;;
	23|*/23)	echo 255.255.254.0	;;
	22|*/22)	echo 255.255.252.0	;;
	21|*/21)	echo 255.255.248.0	;;
	20|*/20)	echo 255.255.240.0	;;
	19|*/19)	echo 255.255.224.0	;;
	18|*/18)	echo 255.255.192.0	;;
	17|*/17)	echo 255.255.128.0	;;

	16|*/16)	echo 255.255.0.0	;;
	15|*/15)	echo 255.254.0.0	;;
	14|*/14)	echo 255.252.0.0	;;
	13|*/13)	echo 255.248.0.0	;;
	12|*/12)	echo 255.240.0.0	;;
	11|*/11)	echo 255.224.0.0	;;
	10|*/10)	echo 255.192.0.0	;;
	9|*/9)		echo 255.128.0.0	;;

	8|*/8)		echo 255.0.0.0		;;
	7|*/7)		echo 254.0.0.0		;;
	6|*/6)		echo 252.0.0.0		;;
	5|*/5)		echo 248.0.0.0		;;
	4|*/4)		echo 240.0.0.0		;;
	3|*/3)		echo 224.0.0.0		;;
	2|*/2)		echo 192.0.0.0		;;
	1|*/1)		echo 128.0.0.0		;;
	0|*/0)		echo 0.0.0.0		;;
	*)		echo 255.255.255.255	;;
	esac
} #End of bits2mask


#-------------------------------------------------------------------------
# broadcastof function, returns the broadcast of the given ip and netmask.
#-------------------------------------------------------------------------
broadcastof () {
#The broadcast is (ip bitwise-or (255.255.255.255-netmask))
	CKPTBROADCASTOF=" broadcastof: Start $1 mask $2" ; #ckpt $CKPTBROADCASTOF

	case $2 in
	32|255.255.255.255)	echo $1									;;
	0|0.0.0.0)			echo 255.255.255.255					;;
	*)
		SPLITIP=$1
		I1O1=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I1O2=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I1O3=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I1O4=$SPLITIP
		case $2 in
		[0-9]|[1-2][0-9]|3[0-2])	SPLITIP=`bits2mask $2`			;;
		*)							SPLITIP=$2						;;
		esac
		I2O1=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I2O2=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I2O3=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I2O4=$SPLITIP
	
		echo $[ $I1O1 | (255-$I2O1) ].$[ $I1O2 | (255-$I2O2) ].$[ $I1O3 | (255-$I2O3) ].$[ $I1O4 | (255-$I2O4) ]
																;;
	esac

	CKPTBROADCASTOF=""
} #End of broadcastof


delline() {
#Params: $1 File that needs the line removed, $2 line to remove (may be a partial line).
    if [ "$#" != "2" ]; then
	echo Incorrect number of arguments to delline! >/dev/stderr
    else
	case "$1" in
	/*)
	    echo Filename is not relative in delline! >/dev/stderr
	    ;;
	*)
	    if [ ! -f "$1" ]; then
		echo "$1" doesn\'t exist, can\'t remove \"$2\". 
	    elif $SUDO cat "$1" | grep -q "^$2\$" ; then
		$SUDO cp -pf "$1" "$1.orig"
		$SUDO cat "$1.orig" | grep -v "^$2\$" | $SUDO dd of="$1" 2>/dev/null
		printf "%1s%-2s%-40s%-50s\n" '-' "$[ `$SUDO cat "$1.orig" | wc -l` - `$SUDO cat "$1" | wc -l` ]" "$1" "$2"
		#Was: echo -n "Removing \"$2\" from $1; " ; echo $[ `$SUDO cat "$1.orig" | wc -l` - `$SUDO cat "$1" | wc -l` ] lines removed.
		$SUDO /bin/rm -f "$1.orig"
	    else
		echo \"$2\" is not in "$1" - not removing. >/dev/stderr
	    fi
	    ;;
	esac
    fi
}

max () {
#Returns the largest of the CLP's, or 0 if none.
    if [ $# -eq 0 ]; then
	echo 0
    else
	MAX=$1
	shift
	while [ -n "$1" ]; do
	    if [ $[ $1 ] -gt $MAX ]; then
		MAX=$[ $1 ]
	    fi
	    shift
	done
	echo $MAX
    fi
}

#-------------------------------------------------------------------------
# networkof function, returns the network of the given ip and netmask.
#-------------------------------------------------------------------------
networkof () {
#Basically, the network is (ip bitwise-and netmask)
	CKPTNETWORKOF=" networkof: Start $1 mask $2" ; #ckpt $CKPTNETWORKOF

	case $2 in
	32|255.255.255.255)	echo $1									;;
	0|0.0.0.0)			echo 0.0.0.0							;;
	*)
		SPLITIP=$1
		I1O1=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I1O2=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I1O3=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I1O4=$SPLITIP
		case $2 in
		[0-9]|[1-2][0-9]|3[0-2])	SPLITIP=`bits2mask $2`			;;
		*)							SPLITIP=$2						;;
		esac
		I2O1=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I2O2=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I2O3=${SPLITIP%%.*} ; SPLITIP=${SPLITIP#*.}
		I2O4=$SPLITIP

		echo $[ $I1O1 & $I2O1 ].$[ $I1O2 & $I2O2 ].$[ $I1O3 & $I2O3 ].$[ $I1O4 & $I2O4 ]
																;;
	esac
	CKPTNETWORKOF=""
} #End of networkof

rpm_add_deps () {
#Given a list of packages, adds the supporting packages in the right order.
#Keep list of reordered rpms; only # them if they're reordered twice or more.
    REORDEREDRPMS=" "

    if [ ! -d var/lib/rpm ]; then
	$SUDO mkdir --parents var/lib/rpm
    fi 

    if [ ! -f $RPMPROVIDES ]; then
#	echo Preparing .provides list >>../debug
        for ONERPM in $RPMDIR/*.rpm ; do
	    $SUDO rpm -qp --provides $ONERPM | sed -e 's/ =.*//' -e "s@^@${ONERPM}\&@" >>$RPMPROVIDES
	    $SUDO rpm -qpl $ONERPM | sed -e "s@^@${ONERPM}\&@" >>$RPMPROVIDES
	done
    fi

    RPMS=""
    for rpm in $* ; do
	RPMS="$RPMS $(rpm_file $rpm)"
    done
    RPMS="$RPMS "

    LISTOK=""
    while [ "$LISTOK" != "YES" ]; do
	#clear >/dev/stderr
	echo >>../debug ; echo Working on `echo $RPMS | sed -e "s@$RPMDIR/@@g"` >>../debug
	LISTOK="YES"
	$SUDO rm -f var/lib/rpm/*
	$SUDO rpm --root $CURRENTDIR --initdb
	for rpm in $RPMS ; do
#	    echo Installed: `$SUDO rpm -qa --root $CURRENTDIR` >>../debug
#	    echo Attempting: rpm -i --root $CURRENTDIR $(rpm_params $DIST $rpm) $(rpm_file $rpm) --justdb --ignoresize --excludedocs >>../debug
	    if ! NEEDEDTEXT="$($SUDO rpm -i --root $CURRENTDIR $(rpm_params $DIST $rpm) $(rpm_file $rpm) --justdb --ignoresize --excludedocs 2>&1 )" ; then
#		if [ "$rpm" = "/usr/src/rh62source/glibc-2.1.3-21.i386.rpm" ]; then
#		    echo NT${NEEDEDTEXT}NT
#		    $SUDO rpm -i --root $CURRENTDIR $(rpm_params $DIST $rpm) $(rpm_file $rpm) -vv
#		fi
		NEEDEDRESOURCE=$(echo "$NEEDEDTEXT" | grep 'is needed by' | head -1 | awk '{print $1}')
		if [ -n "$NEEDEDRESOURCE" ]; then
		    export NEEDEDRESOURCE
		    if [ `cat $RPMPROVIDES | grep "&$NEEDEDRESOURCE *$" | wc -l` -ne 1 ]; then
			echo '0 or 2+ rpms provide ' $NEEDEDRESOURCE .  Using the first one of: >>../debug
			cat $RPMPROVIDES | grep "&$NEEDEDRESOURCE *$" >>../debug
		    fi
		    NEEDEDRPM=$(cat $RPMPROVIDES | grep "&$NEEDEDRESOURCE *$" | head -1 | sed -e 's@&.*@@') #; echo NRPM "$NEEDEDRPM" >>../debug
#		    echo "$NEEDEDRPM" is needed to satisfy "$rpm" . >>../debug
		    if echo "$RPMS" | grep -q "[# ]$NEEDEDRPM[# ]" ; then
			echo Stripping $NEEDEDRPM to reorder before $rpm . >>../debug
			RPMS=`echo "$RPMS" | sed -e "s@[# ]$NEEDEDRPM@@"`
			if echo "$REORDEREDRPMS" | grep -q " $NEEDEDRPM "; then	#If it's already been reordered, use # to link them
			    echo -n '#' >>../debug
			    RPMS=`echo "$RPMS" | sed -e "s@ $rpm @ $NEEDEDRPM#$rpm @"`
			else
			    echo -n '-' >>../debug
			    RPMS=`echo "$RPMS" | sed -e "s@ $rpm @ $NEEDEDRPM $rpm @"`
			    REORDEREDRPMS=" $REORDEREDRPMS $NEEDEDRPM "
			fi
#			echo ${NEEDEDRPM}: $REORDEREDRPMS >>../debug
			sleep 1
		    else
			RPMS=`echo "$RPMS" | sed -e "s@ $rpm @ $NEEDEDRPM $rpm @"`
		    fi
#		    if [ "$rpm" = "/usr/src/rh62source/info-4.0-5.i386.rpm" ]; then
#			echo
#			echo X${NEEDEDRESOURCE}X
#			echo X${NEEDEDRPM}X
#			echo "X${NEEDEDTEXT}X"
#			read JUNK	#sleep 4
#		    fi
		    LISTOK="NO"
		    continue 2
		fi
	    fi
	done
    done
    $SUDO rm -f ./var/lib/rpm/*
    $SUDO rmdir var/lib/rpm var/lib var
    #$SUDO rpm --root $CURRENTDIR --initdb
    echo $RPMS
    echo >>../debug
    echo $RPMS >>../debug
#    ( for ONERPM in $RPMS ; do
#    	echo $ONERPM | sed -e "s@$RPMDIR/@@g"
#    done ) | sort >>../debug
}


rpm_params() {
#Returns the custom parameters needed to install an rpm.
#Params: distribution name, package name
#RedHat:
#	libtermcap and (bash, I think) depend on each other.
#	kernel needed by util-linux, but we'll just nodeps it.
#	tcpdump wants a 2.2.0 kernel or higher as well
#Mandrake: the following pairs depend on each other:
#	msec and chkconfig
#	gpm and ncurses
#	freetype and XFRee86-libs
#	xinitrc and XFree86
#	xterm and XFree86
#	kernel needed by util-linux, but we'll just nodeps it.
#	Mesa and Mesa-common
#Caldera 2.4
#	lisa and bash
#	libpwdb and bash
#	cracklib and bash
#	libpam and bash
#	fileutils and bash
#	sh-utils and bash
#	util-linux and bash
#	sed and bash
#	SysVinit and SysVinit-scripts
    RPMOPTIONS=''
#ZZZZ
#Add them back in, one at a time, if and when needed!
#ZZZZ
#    for ONERPMBASE in `echo $* | tr '#' ' '` ; do
#	case $1 in
#	rh*|im*)
#    	    case $ONERPMBASE in
#    	    libtermcap*|util-linux*|tcpdump*|Mesa*|devfsd*|modutils*)
#		RPMOPTIONS=" $RPMOPTIONS --nodeps "
#		;;
#	    esac
#	    ;;
#	md*)
#    	    case $ONERPMBASE in
#    	    util-linux*|msec*|gpm*|freetype*|xinitrc*|xterm*|Mesa-common*)
#		RPMOPTIONS=" $RPMOPTIONS --nodeps "
#		;;
#	    esac
#	    ;;
#	cl*)
#    	    case $ONERPMBASE in
#    	    #lisa*|libpwdb*|cracklib*|libpam*|fileutils*|sh-utils*|util-linux*|sed*|SysVinit-scripts*)
#	    #    RPMOPTIONS=" $RPMOPTIONS --nodeps "
#	    #    ;;
#	    lisa*)
#		RPMOPTIONS=" $RPMOPTIONS --noorder "
#		;;
#	    esac
#	    ;;
#	co*)
#	    case $ONERPMBASE in
#	    libtermcap*|tcpdump*)
#		RPMOPTIONS=" --nodeps "
#		;;
#	    esac
#    	    ;;
#	esac
#    done
    echo $RPMOPTIONS
}

rpm_file() {
    if [ -n "`echo $* | grep '#'`" ]; then
	NEWSEPCHAR=' '
    else
	NEWSEPCHAR='#'
    fi

    SEPCHAR=' '		#we have to separate this block from previous blocks
    for ONERPMBASE in `echo $* | tr '#' ' '` ; do
        case $ONERPMBASE in
        *.rpm)
    	    echo -n "$SEPCHAR$ONERPMBASE"
	    ;;
	*)
    	    local name=$ONERPMBASE
    	    #set $RPMDIR/$ONERPMBASE[-0-9.i]*.rpm	#Finds false dupe on fvwm2-... and fvwm2-icons...
    	    set $RPMDIR/$ONERPMBASE[-0-9]*.rpm
    	    if [ $# -ne 1 ]; then
		echo >/dev/stderr
		echo Found too many rpms for $name >/dev/stderr
        	echo -n "$SEPCHAR$1"
    	    elif [ ! -f $1 ]; then
		echo >/dev/stderr
		echo Found no rpms for $name >/dev/stderr
	    else
        	echo -n "$SEPCHAR$1"
    	    fi
	    ;;
	esac
	SEPCHAR="$NEWSEPCHAR"
    done
}

substline() {
#Params: $1 File that needs the additional line, $2 string to look for, $3 string with which it should be replaced.
    if [ "$#" != "3" ]; then
	echo Incorrect number of arguments to substline! >/dev/stderr
    else
        case "$1" in
	/*)
	    echo Filename is not relative in substline! >/dev/stderr
	    ;;
	*)
	    if [ ! -f "$1" ]; then
		$SUDO touch "$1"
	    fi
	    if $SUDO cat "$1" | grep -q "$2" ; then
		printf "%-3s%-40s%-50s\n" '-/+' "$1" "$2 -> $3"	#Was: echo Replacing \"$2\" with \"$3\" in $1
		$SUDO cp -pf "$1" "$1.orig"
		$SUDO cat "$1.orig" | sed -e "s@$2@$3@g" | $SUDO dd of="$1" 2>/dev/null
		$SUDO rm -f "$1.orig"
	    fi
	    ;;
	esac
    fi
}

function get_data()
{
    local prompt=$1
    local default=$2
    local verify=$3
    local answer=""
    local err_msg

    while true; do
        echo -n "$prompt [$default]: " 1>&2
        read answer
        [ "$answer" = "" ] && answer=$default
	[ $verify != "" ] && err_msg=`$verify $answer` && break
	[ "$err_msg" != "" ] && echo -e $err_msg 1>&2
    done
    echo $answer
}

function verify_yn()
{
    local answer=$1
    local res=true

    echo "Please answer 'y' or 'n'"
    [ "$1" = "y" ] || [ "$1" = "n" ]
}

function verify_fs()
{
    local file=$1;
    local res=true
    local ok="y"

    if [ -b "$file" ]
    then
	ok=`get_data "\"$file\" is a block device - confirm that you want to install into it" y verify_yn`
    elif [ -d "$file" ]
    then
	ok=`get_data "\"$file\" is a directory - confirm that you want to install into it" y verify_yn`
    elif [ -f "$file" ]
    then
	ok=`get_data "\"$file\" already exists - OK to delete it" y verify_yn`
    elif [ -e $file ]
    then
	ok=`get_data "\"$file\" is not a plain file - OK to delete it" y \
	    verify_yn`
    fi
    [ "$ok" = "y" ]
}

function verify_size()
{
    local size=$1
    local err=""
    local space=`df \`dirname $FSFILE\` | tail -1 | awk '{print $4}'`
    space=$(( $space / 1024 ))
    local mnt=`df \`dirname $FSFILE\` | tail -1 | awk '{print $6}'`

    if [ `expr "$size" : '[0-9]*'` -ne `expr length "$size"` ]
    then
	err="The size must be a number, containing only digits"
    elif [ "$size" -lt 100 ]
    then
	err="The size should be at least 100"
    elif [ $space -lt "$size" ]
    then
	err="There is not enough free space in $mnt (available space : \
$space M)"
    fi
    echo $err
    [ "$err" = "" ]
}

function verify_mnt()
{
    local dir=$1
    local err=""

    if [ ! -d "$dir" ]
    then
	err="$dir is not a directory"
    elif [ ! -d "$dir/RedHat/RPMS" ]
    then
	err="$dir seems not to be a Red Hat distribution ($dir/RedHat/RPMS \n\
doesn't exist or isn't a directory)"
    fi
    echo $err
    [ "$err" = "" ]
}

function verify_sudo()
{
    local sudo=$1
    local err=""

    if [ "$sudo" = "" ]
    then err=""
    elif [ ! -f "$sudo" ] || [ ! -x "$sudo" ] || [ ! -u "$sudo" ]
    then
	err="\"$sudo\" is not an setuid root executable file"
    fi
    echo $err
    [ "$err" = "" ]
}
