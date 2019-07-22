#!/bin/sh
# test: Version management : backup files restore testing
set +x

cd ../../../../../test/mnt/bkpfs/
rm -f in.test.$$
echo dummy hello1 >> in.test.$$
echo dummy hello2 >> in.test.$$
echo dummy hello3 >> in.test.$$
echo dummy hello4 >> in.test.$$

../../../usr/src/hw2-vbansal/CSE-506/./bkpctl -r "2" in.test.$$

# now verify that the restore is successful or not
if cmp ../../some/lower/path/in.test.$$ ../../some/lower/path/in.test.$$2.tmp ; then
        echo Test13 passes
        exit 0
else
        echo Test13 fails
        exit 1
fi

