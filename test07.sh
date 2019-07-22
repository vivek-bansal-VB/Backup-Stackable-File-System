#!/bin/sh
# test: Invalid arg passed to the delete option
set +x
echo dummy test > in.test.$$
.././bkpctl -d "random" in.test.$$
retval=$?
if test $retval != 0 ; then
        echo test7 passes
        exit $retval
else
        echo test7 fails
fi
