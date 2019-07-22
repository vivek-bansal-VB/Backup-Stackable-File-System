#!/bin/sh
# test : Invalid flags entered into the command line 
set +x
echo dummy test > in.test.$$
.././bkpctl -a in.test.$$
retval=$?
if test $retval != 0 ; then
	echo Test1 passes
	exit $retval
else
	echo Test1 fails
fi
