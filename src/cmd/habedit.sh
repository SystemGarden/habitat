#!/bin/sh
#
# Script to edit a route object and save it back to the same location.
# The user's favorite editor will be used, although the data will generally
# tabular in structure which should be preserved.
#
# If there is one argument containing a colon, it is assumed that
# it is a route address.
# If there are multiple arguments, then it is assumed that the first is 
# a file name and the second a ring and the third a duration.
# The editor is obtained by firstly looking at environment variable VISUAL,
# followed by EDITOR and finally using the default editor `vi'.
#
# Nigel Stuckey, Oct 2004
# Copyright System Garden Ltd 2004. All rights reserved.

CMD=`basename $0`
DIR=`dirname $0`
HOST=`hostname`
EDFILE=/tmp/$CMD.$$
TMPFILE=/tmp/$CMD.$$.tmp
PATH=/usr/bin:/bin:$DIR; export PATH
DEFED=vi
ED=${VISUAL:-${EDITOR:-$DEFED}}

trap 'echo "Aborted: nothing saved"; rm -f $EDFILE; exit 1' 1 2 3 13 15

if [ $# -eq 1 ]
then
    if [ `expr match "$1" ".*:"` -ne 0 ]
    then
	RT="$1"
    fi
fi

if [ -z "$RT" ]
then
    if [ $# -ne 3 ]
    then
	echo "usage: $0 <route>"
	echo "   or  $0 <file> <ring> <duration>"
	exit 1
    fi
    if [ ! -f "$1" ]
    then
	echo "$0: expected store ($1) does not exist";
	exit 2;
    fi

    RT="rs:$1,$2,$3"
fi

if [ -z "$ED" ]
then
    echo "$0: unable to find editor";
    exit 3;
fi

if habget -i $RT > $EDFILE;
then
    echo "please wait..."
    shalledit="y"
else
    read -p "Would you like to create new route object (y or n)? " new
    if [ "$new" = "y" -o "$new" = "Y" ]
    then
	rm -f $EDFILE
	touch $EDFILE
	shalledit="y"
    else
	echo "Nothing created"
	shalledit="n"
    fi
fi

if [ "$shalledit" = "y" ]
then
    if [ "`head -1 $EDFILE`" = "_time	_seq	text" ]
    then
	# free text mode, chop off the headers, ruler as well as
	# seq and time
	tail +3 $EDFILE | cut -f 3 >$TMPFILE
	mv $TMPFILE $EDFILE
	FREETEXT=1
    fi
    if $ED $EDFILE;
    then
	if [ $FREETEXT ]
	then
	    cat $EDFILE | ./habput -f $RT
	else
	    cat $EDFILE | ./habput $RT
	fi
    fi
fi

rm -f $EDFILE $TMPFILE
