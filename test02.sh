#!/bin/sh -x
# test: File missing from the command line
set +x
echo dummy test > in.test.$$
.././bkpctl -l
retval=$?
if test $retval != 0 ; then
        echo Test2 passes
        exit $retval
else
        echo Test2 fails
fi
