#!/bin/sh
# test: Version management : backup files visibility policy testing
set +x

cd ../../../../../test/mnt/bkpfs/
rm -f in.test.$$
echo dummy hello1 >> in.test.$$
echo dummy hello2 >> in.test.$$
echo dummy hello3 >> in.test.$$
echo dummy hello4 >> in.test.$$

for file in "$(ls)"
do
	if [ "$file" == "*.tmp" ];
	then
		echo "Test15 fails"
	fi
done

echo "Test15 passes"
