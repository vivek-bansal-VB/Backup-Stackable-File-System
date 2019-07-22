#!/bin/sh
# test: No flags passes to the command line
set +x
	
.././bkpctl
retval=$?
if test $retval != 0 ; then
        echo Test3 passes
        exit $retval
else
        echo Test3 fails
fi
