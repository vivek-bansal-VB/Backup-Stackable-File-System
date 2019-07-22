#!/bin/sh
# test: backup files creation testing
set +x

cd ../../../../../test/mnt/bkpfs/
rm -f in.test.$$
echo dummy hello1 >> in.test.$$
echo dummy hello2 >> in.test.$$
echo dummy hello3 >> in.test.$$

../../../usr/src/hw2-vbansal/CSE-506/./bkpctl -l in.test.$$

retval=$?
if test $retval == 0 ; then
        echo Test10 passes
        exit $retval
else
        echo Test10 fails
fi
