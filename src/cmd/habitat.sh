#!/bin/sh
#
# init.d script to start and stop the habitat daemon (called clockwork)
# and find its status
#
# Nigel Stuckey, December 2004
# Copyright System Garden Ltd 2004, All rights reserved
#
# Red Hat's chkconfig system
# chkconfig: 2345 92 8
# description: Application and system performance monitor, \
#              collecting and visualising trends, availability \
#              and service levels
# processname: clockwork
#
# LSB's init system
### BEGIN INIT INFO
# Provides: clockwork
# Required-Start: $local_fs $network $syslog
# Should-Start: nthd
# Required-Stop:
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Habitat server monitor
# Description: Application and system performance monitor, \
#              collecting and visualising trends, availability \
#              and service levels
### END INIT INFO
#
# Source function library.
. /etc/init.d/functions

RETVAL=0
label="habitat"
prog="/usr/bin/clockwork"
sprog=`basename $prog`
progstop="/usr/bin/killclock"
datadir="/var/lib/habitat"
lockfile="/var/run/clockwork.run"

if [ ! -x "$prog" ]; then
    gprintf "no executable called $prog";
    exit 0;
fi

# See how we were called

start() {
	if [ ! -f "$lockfile" ]; then 
	    gprintf "Starting %s: " "$label"
	    daemon --user daemon clockwork
	    RETVAL=$?
	    echo
	fi
	return $RETVAL
}	

stop() {
	gprintf "Stopping %s: " "$label"
	"$progstop"
	RETVAL=$?
	echo
	return $RETVAL
}

status_hab() {
	#if [ ! -f "$lockfile" ]; then 
	#    read pid uid term stime < "$lockfile"
	#fi
	status "$sprog"
}

restart() {
	stop
	start
}

case "$1" in
  start)
  	start
	;;
  stop)
  	stop
	;;
  status)
  	status_hab
	;;
  restart|reload)
  	restart
	;;
  *)
	gprintf "Usage: %s {start|stop|status|restart}\n" "$0"
	exit 1
esac

exit $?

