#!/bin/sh
# test: Version management : Retention policy testing, keeping only 10 backups at a time
set +x

cd ../../../../../test/mnt/bkpfs/
rm -f in.test.$$
echo dummy hello1 >> in.test.$$
echo dummy hello2 >> in.test.$$
echo dummy hello3 >> in.test.$$
echo dummy hello4 >> in.test.$$
echo dummy hello5 >> in.test.$$
echo dummy hello6 >> in.test.$$
echo dummy hello7 >> in.test.$$
echo dummy hello8 >> in.test.$$
echo dummy hello9 >> in.test.$$
echo dummy hello10 >> in.test.$$
echo dummy hello11 >> in.test.$$
echo dummy hello12 >> in.test.$$
echo dummy hello13 >> in.test.$$
echo dummy hello14 >> in.test.$$
echo dummy hello15 >> in.test.$$

../../../usr/src/hw2-vbansal/CSE-506/./bkpctl -l in.test.$$

for file in "$(ls)"
do
	echo "$file"
        if [ "$file" == "in.test.$$1.tmp" -o "$file" == "in.test.$$2.tmp" -o "$file" == "in.test.$$3.tmp" -o "$file" == "in.test.$$4.tmp" -o "$file" == "in.test.$$5.tmp" ]
        then
                echo "Test14 fails"
        fi
done

echo "Test14 passes"
