#! /bin/bash -fe
#
# ----------------------------------------------------------------------
#
# Simple bash script to build zingzong for various platforms
#
# by Benjamin Gerard AKA Ben/OVR
#
# ----------------------------------------------------------------------

top=$(realpath $(dirname "$0")) # Where this script is located.
[ -n "${arch}" ] ||		# Host triplet
    arch="${PWD##*/}"		# - Default to current dir name
[ -n "${ZZ_MAKEFILE}" ] ||	# Sub project makefile
    ZZ_MAKEFILE=../src/Makefile	# - Default to zingzong cli program

echo ZZ_MAKEFILE=$ZZ_MAKEFILE

DEPLIBS="AO SRATE SOXR"
vars1=( CC LD STRIP PKGCONFIG INSTALL DEBUG PROFILE MAKERULES )
vars3=( CPPFLAGS CFLAGS LDFLAGS LDLIBS gb_CFLAGS gb_LDLIBS gb_LDFLAGS
	prefix )
vars2=( "${var3[@]}" )
for dep in $DEPLIBS; do
    vars2+=( NO_${dep} ${dep}_CFLAGS ${dep}_LIBS )
done

# ----------------------------------------------------------------------
# Print help message on -h --help or --usage
#
Usage() {
    cat <<EOF
 Usage: build.sh -j
        build.sh [--no-env] [VAR=VAL ...] [make-args ...]

   This zingzong build script MUST to be call from one of the host
   child directory.

   e.g: mkdir i686-pc-cygwin; cd i686-pc-cygwin; ../build.sh

   All command line arguments are propagated to make however VAR=VAL are
   intercepted and might be modified.

   The following environment variables are used unless --no-env is set:

EOF
    x='  '
    for var in "${vars1[@]}" "${vars3[@]}"; do
	y="${x}${x:+ }${var}"
	if [ ${#y} -gt 78 ]; then
	    echo "$x"; x="   ${var}"
	else
	    x="$y"
	fi
    done
    echo "$x"

    cat <<EOF

   For each dependency library name in {${DEPLIBS// /,}}:

   NO_{name} ....... Disable(0)/Enable(1)
   {name}_CFLAGS ... How to compile (usually -I)
   {name}_LIBS ..... How to link (usually -L and -l)

EOF
    exit 0
}

# ----------------------------------------------------------------------
# Parse command line arguments
#
unset first
for arg in "$@"; do
    if [ s${first+et} != set ]; then
	first=x; set --
    fi
    
    case "$arg" in
	-h | --help | --usage)
	    Usage
	    ;;
	--no-env)
	    unset ${vars1[@]} ${vars2[@]}
	    for dep in $DEPLIBS; do
		eval unset NO_${dep} ${dep}_CFLAGS ${dep}_LIBS
	    done
	    ;;
	    
	[[:alpha:]][_[:alnum:]]+)
	    var="${arg%%=*}"
	    val="${arg#*=}"
	    eval $var='"$val"'
	    ;;
	*)
	    set -- "$@" "$arg"
	    ;;
    esac
done

if [ s${MAKERULES+et} != set -a -r makerules ]; then
    MAKERULES=makerules
fi

case "$arch" in
    *-*-*) ;;
    *) echo "build.sh: build directory is not a valid host-triplet." >&2
       exit 1 ;;
esac

# ----------------------------------------------------------------------
# Cross-Compile
#
crosscompile=no
gccarch=`gcc -dumpmachine`
test "${gccarch}" = "${arch}" || crosscompile=yes
echo "cross-compiling: ${crosscompile}"

if [ ${crosscompile} = yes ]; then
    ## ---------------------------------------------------
    ## By default setup a somewhat standard gnu/toolchain
    ## cross-compile environment
    ## ---------------------------------------------------
    
    ## CC
    if [ -z "${CC-}" ]; then
	CC="${arch}"-gcc
	xx=`which "$CC"`
	echo "CC=\"$xx\""
    fi

    ## LD
    if [ -z "${LD-}" ]; then
	LD="${arch}"-ld
	xx=`which "$LD"`
	echo "LD=\"$xx\""
    fi

    ## STRIP
    if [ -z "${STRIP-}" ]; then
	STRIP="${arch}"-strip;
	xx=`which "$STRIP"`
	echo "STRIP=\"$xx\""
    fi
    
    ## PKGCONFIG
    if [ -z "${PKGCONFIG-}" ]; then
	PKGCONFIG="${arch}"-pkg-config
	xx=`which "$PKGCONFIG"`
	echo "PKGCONFIG=\"$xx\""
    fi
    
fi

# ----------------------------------------------------------------------
# Architecture specific
#
case "$arch" in
    *-w64-mingw32)
	gb_CFLAGS+="${gb_CFLAGS:+ }-static"
	gb_LDFLAGS+="${gb_LDFLAGS:+ }-static -static-libgcc" ;;
    *-*-*)
	true ;;
    *)
	echo "build.sh: can not build for host -- ${arch}" >&2
	exit 1 ;;
esac

# ----------------------------------------------------------------------
# Dependency libraries
#
for var in $DEPLIBS; do
    if eval test s\${NO_${var}+et} = set -a x\"\$NO_${var}\" != x0
    then
	eval unset ${var}_CFLAGS ${var}_LIBS
	eval NO_${var}=1
    fi
done

# ----------------------------------------------------------------------
# Variables that CAN NOT be empty
#
for var in ${vars1[@]}; do
    if eval test -n \"\${$var-}\"; then
	eval set -- '"$@"' \"$var=\${$var}\"
    fi
done

# ----------------------------------------------------------------------
# Variables that CAN be empty
#
for var in ${vars2[@]}; do
    if eval test s\${$var+et} = set; then
	eval set -- '"$@"' \"$var=\${$var}\"
    fi
done

# ----------------------------------------------------------------------
# Let's do this
#
set --\
    -B -f ${top}/${ZZ_MAKEFILE}\
    "$@"

echo "> make"
for arg in "$@"; do
    echo "+ \"$arg\""
done
echo
make "$@"
version=`$SHELL ${top}/../src/vcversion.sh`

out="# ${project:-zingzong} ${version} for ${arch} #" 
cat <<EOF

 ${out//?/#}
 ${out}
 ${out//?/#}
EOF
