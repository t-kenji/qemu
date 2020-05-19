#!/bin/bash

PROGNAME="build-prepare.sh"


if echo "$0" | grep -q "$PROGNAME"; then
    echo -e "\033[31m[ERROR] This script needs to be sourced.\033[m"
    SCRIPT_PATH=`readlink -f $0`
    echo "Try run command \". $SCRIPT_PATH -h\" to get help."
    unset SCRIPT_PATH PROGNAME
    exit
else
    ROOTDIR="`readlink -f $BASH_SOURCE | xargs dirname`"
    if ! [ -e "$ROOTDIR/$PROGNAME" ]; then
        echo "\033[31mGo to where $PROGNAME locates, then run: "
             ". $PROGNAME <args>\033[m"
        unset ROOTDIR PROGNAME
        return
    fi
fi

cleanup() {
    unset PROGNAME ROOTDIR BUILDDIR TARGETS\
          setup_flag setup_builddir setup_h setup_error
    unset -f usage cleanup
}

usage() {
    echo "Usage: . $PROGNAME -b <path>"
    echo "    Optional parameters:
    * [-b path]: path of project build folder.
    * [-h]:      help
"
}

OLD_OPTIND=$OPTIND
while getopts "b:h" setup_flag; do
    case $setup_flag in
    b) setup_builddir="$OPTARG";;
    h) setup_h='true';;
    ?) setup_error='true';;
    esac
done
OPTIND=$OLD_OPTIND

if [ "$setup_h" = "true" -o "$setup_error" = "true" ]; then
    usage && cleanup && return
fi

if [ "$setup_builddir" = "" ]; then
    usage && cleanup && return
fi

TARGETS="aarch64-softmmu,arm-softmmu,ppc-softmmu,ppc64-softmmu"

if echo $setup_builddir | grep -q ^/; then
    BUILDDIR="$setup_builddir"
else
    BUILDDIR="$(pwd)/$setup_builddir"
fi

mkdir -p $BUILDDIR
cd $BUILDDIR

CONFIG_SCRIPT=$BUILDDIR/reconfigure.sh
cat << __EOS__ > $CONFIG_SCRIPT
echo "start configure .."
$ROOTDIR/configure \\
    --prefix=/usr \\
    --target-list=$TARGETS \\
    --enable-debug \\
    --enable-debug-tcg \\
    --enable-debug-info \\
    --enable-trace-backend=log \\
    --enable-gtk \\
    --enable-spice \\
    --enable-vte \\
    --enable-fdt \\
    --enable-virtfs \\
    --enable-attr \\
    --enable-kvm \\
    \$NULL
__EOS__
chmod +x $CONFIG_SCRIPT
$CONFIG_SCRIPT

cleanup
