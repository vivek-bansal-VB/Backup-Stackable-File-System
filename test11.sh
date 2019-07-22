#!/bin/sh
# test: Version management : backup files view testing
set +x

cd ../../../../../test/mnt/bkpfs/
rm -f in.test.$$
echo dummy hello1 >> in.test.$$
echo dummy hello2 >> in.test.$$
echo dummy hello3 >> in.test.$$
echo dummy hello4 >> in.test.$$

../../../usr/src/hw2-vbansal/CSE-506/./bkpctl -v "3" in.test.$$

retval=$?
if test $retval == 0 ; then
        echo Test11 passes
        exit $retval
else
        echo Test11 fails
fi
