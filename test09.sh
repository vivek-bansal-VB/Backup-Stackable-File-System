#!/bin/sh
# test: File don't exist error
set +x
.././bkpctl -l ../../../../test/mnt/bkpfs/infile.test
retval=$?
if test $retval != 0 ; then
        echo test9 passes
        exit $retval
else
        echo test9 fails
fi
