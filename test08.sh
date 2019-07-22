#!/bin/sh
# test: Invalid argument passed to the restore option
set +x
echo dummy test > in.test.$$
.././bkpctl -r "oldest" in.test.$$

retval=$?
if test $retval != 0 ; then
        echo test8 passes
        exit $retval
else
        echo test8 fails
fi
