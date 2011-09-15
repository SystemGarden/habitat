#!/bin/sh
#
# Simple script to start a session of replication immediately from the 
# command line rather than waiting for the replication job to start within 
# clockwork.
#
# Nigel Stuckey, June 2004
# Copyright System Garden Ltd 2004-5. All rights reserved.

CMD=`basename $0`
DIR=`dirname $0`

MYDIR=`pwd`
cd $DIR
PATH=`pwd`:/usr/bin:/bin; export PATH
cd $MYDIR

echo "Replicating to repository now..."

habmeth $* replicate replicate.in replicate.out "grs:%v/%h.rs,rstate,0"

RETURN=$?;

if [ $RETURN -ne 0 ];
then
    echo "$0: replication failed";
fi

exit $RETURN;
