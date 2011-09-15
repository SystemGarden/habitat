#!/bin/sh
#
# Script to find the status of clockwork
#
# Nigel Stuckey, May 2010
# Copyright system garden Ltd 2010. All rights reserved.

RUNFILE1="/var/lib/habitat/clockwork.run"
RUNFILE2="/tmp/clockwork.run"

# Read out the data
if [ -f "$RUNFILE1" ]
then
	RUNFILE="$RUNFILE1";
else
	if [ -f "$RUNFILE2" ]
	then
		RUNFILE="$RUNFILE2";
	else
	        echo "Clockwork not running"
		echo "  no lock file in $RUNFILE1 or $RUNFILE2";
		exit 2;
	fi
fi

read pid uid term stime < $RUNFILE
# confirm process is alive by looking at process table
psout=`ps -p $pid | tail -n +2`

if [ -n "$psout" ]
then
    echo "Clockwork process $pid is running"
    echo "  was started at $stime, user $uid, on $term"
else
    echo "Clockwork has stopped: removing lock file"
    echo "  had started at $stime, user $uid, on $term"
    rm -f $RUNFILE
fi

exit 0;
