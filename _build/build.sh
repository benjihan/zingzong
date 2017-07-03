#! /bin/bash -e
#
# ----------------------------------------------------------------------
#
# Simple bash script to build zingzong for various platforms
#
# by Benjamin Gerard
#
# ----------------------------------------------------------------------

## Print help message on -h --help or --usage
#
case "$1" in
    -h | --help | --usage)
	cat <<EOF
Call this script from one of the host directory.

  All command line arguments are propagated to make.

  The following environment variables are used:

  CC .......... compiler
  CFLAGS ...... CFLAGS to use
  PKGCONFIG ... pkg-config to call (unless AO_LIBS is set)
  AO_LIBS ..... How to link libao
  AO_CFLAGS ... How to compile with libao (required if AO_LIBS is set)
  DEBUG ....... Set to use the default debug CFLAGS (unless CFLAGS is set)

EOF
	exit 0;;
esac

top="${PWD%/*}"		      # Where this script is located
arch="${PWD##*/}"	      # basename of dirname is our host name

## unless CC is defined fallback to {host}-gcc
#
if [ s${CC+et} != set ]; then
    CC="${arch}"-gcc
fi

## unless CFLAGS are set use optimized or debug settings for gcc
#
if [ s${CFLAGS+et} != set ]; then
    if [ s${DEBUG+et} != set ]; then
	CFLAGS='-O3 -Wall'
    else
	CFLAGS="-O0 -g -DDEBUG=${DEBUG:-1} -Wall"
    fi
fi

## Unless PKGCONFIG is set use {hosT}-pkg-config
#
if [ s${PKGCONFIG+et} != set ]; then
    PKGCONFIG="${arch}"-pkg-config
fi

## How we build some known targets
#
case "$arch" in
    *-w64-mingw32)
	CFLAGS="${CFLAGS} -static -static-libgcc" ;;
    *-*-*)
	true ;;
    *)
	echo "build.sh: can not build for host -- ${arch}" >&2
	exit 1 ;;
esac

## AO_CFLAGS / AO_LIBS
#
if [ s${AO_CFLAGS+et} = set ]; then
    set -- "$@" AO_LIBS="$AO_LIBS" AO_CFLAGS="$AO_CFLAGS"
fi

## SRATE_CFLAGS / SRATE_LIBS
#
if [ s${SRATE_CFLAGS+et} = set ]; then
    set -- "$@" SRATE_LIBS="$SRATE_LIBS" SRATE_CFLAGS="$SRATE_CFLAGS"
fi

## Other variables of interest
#
for var in CC CFLAGS PKGCONFIG; do
    if eval test s\${$var+et} = set; then
	eval set -- '"$@"' \"x_$var=\${$var}\"
    fi
done

make \
    -B -f ${top}/../src/Makefile\
    CC="$CC" CFLAGS="$CFLAGS" PKGCONFIG="$PKGCONFIG" \
    "$@"

version=`$SHELL ${top}/../src/vcversion.sh`
echo "zinzong ${version} for ${arch}"
