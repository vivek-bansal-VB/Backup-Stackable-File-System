#!/bin/sh
# test: d option passes without args
set +x
echo dummy test > in.test.$$
.././bkpctl -d in.test.$$
retval=$?
if test $retval != 0 ; then
        echo Test6 passes
        exit $retval
else
        echo Test6 fails
fi
