#!/bin/sh
#
# Script to shutdown clockwork, uses /tmp/hab.clockwork for the pid
#
# Nigel Stuckey, July 2003
# Copyright system garden Ltd 2003. All rights reserved.

RUNFILE1="/var/lib/habitat/clockwork.run"
RUNFILE2="/tmp/clockwork.run"

trap 'echo "Aborted: clockwork not stopped"; exit 1' 1 2 3 13 15

if [ -f "$RUNFILE1" ]
then
	RUNFILE="$RUNFILE1";
else
	if [ -f "$RUNFILE2" ]
	then
		RUNFILE="$RUNFILE2";
	else
		echo "$0: lock file does not exist; is clockwork running?";
		echo "(looking for $RUNFILE1 or $RUNFILE2)";
		exit 2;
	fi
fi

read pid uid term stime < $RUNFILE

echo "Stopping Clockwork"
echo "  pid $pid, user $uid and started on $term at $stime"
if kill $pid
then
    echo "Stopped"
else
    echo "Assuming clockwork had stopped: removing lock file"
    rm -f $RUNFILE
fi

exit 0;
