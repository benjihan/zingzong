#!/bin/sh -e
#
# by Benjamin Gerard
#

export LC_ALL=C

if [ -r ./VERSION ]; then
    cat ./VERSION
else
    cd "$(dirname $0)"
    if [ -r ./VERSION ]; then
	cat ./VERSION
    else
	tagname=$(git tag -l 'v[0-9]*' --sort=-tag -i --no-column | head -n1)
	tweaks=$(git rev-list --count HEAD ^$tagname)
	if [ "${tweaks}" = 0 ]; then
	    tweaks=''
	else
	    tweaks=".${tweaks}"
	fi	    
	echo "$tagname" |
	    sed -e "s/^v\([0-9].*\)/\1${tweaks}/"
    fi
fi
