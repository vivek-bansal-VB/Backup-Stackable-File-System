#!/bin/sh
# test: -r flag without args
set +x
echo dummy test > in.test.$$
.././bkpctl -r in.test.$$
retval=$?
if test $retval != 0 ; then
        echo Test5 passes
        exit $retval
else
        echo Test5 fails
fi
