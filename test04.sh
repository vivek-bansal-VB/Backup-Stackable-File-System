#!/bin/sh
# test: -v option passed without args
set +x
echo dummy test > in.test.$$
.././bkpctl -l in.test.$$
retval=$?
if test $retval != 0 ; then
        echo Test4 passes
        exit $retval
else
        echo Test4 fails
fi
