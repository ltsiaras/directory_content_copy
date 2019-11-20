#!/bin/bash

# $1 -- the server's executable file.
# $2 -- the client's executable file.
# $3 -- the number of clients.
# $4 -- the server's listening port.

# Verify that the command line arguments are correct.
if [ "$#" -ne 4 ]; then
	echo "Wrong number of arguments!" >&2;
	echo "Usage: ./testExecution.sh <server_exe> <client_exe> <num_of_clients> <server_port>" >&2;
	echo "Exiting..." >&2;
	exit -1;
fi

numberREGEX='^[0-9]+$'
if ! [[ $3 =~ $numberREGEX ]]
then
	echo "The 3rd argument must be a number!" >&2;
	echo "Exiting..." >&2;
	exit -1;
fi

if ! [[ $4 =~ $numberREGEX ]]
then
	echo "The 4th argument must be a number!" >&2 ;
	echo "Exiting..." >&2;
	exit -1;
fi

if [ "$3" -le "0" ]
then
	echo "The 3rd argument must be a positive number!" >&2;
	echo "Exiting..." >&2;
	exit -1;
fi

if [ "$4" -le "0" ]
then
	echo -e "The 4th argument must be a positive number, preferably bigger than 12000!" >&2;
	echo "Exiting..." >&2;
	exit -1;
fi
# Ending argument checking.

# Copy both executable files to the /tmp directory.
cp $1 /tmp
cp $2 /tmp

cd /tmp

# Create a sample hierarchy.
mkdir Server; cd Server/
fallocate -l 1K file1
fallocate -l 2K file2
fallocate -l 4K file3

mkdir Folder1; cd Folder1/
fallocate -l 8K file4
fallocate -l 16K file5
fallocate -l 32K file6

mkdir Folder2; cd Folder2/
touch empty
fallocate -l 256K file7
fallocate -l 512K file8
fallocate -l 1M file9

cd /tmp

# Create a separate folder for each client and copy the client's
# executable file in it.
for (( i=1; i <= $3; i++ ))
do
	mkdir "Client_"$i
	cp $2 "Client_"$i
done

# Start the server.
./dataServer -s 5 -q 10 -p $4 </dev/null &>/dev/null &

serverPID=$!
echo "Server successfully started!"

# Sleep for some seconds, in order to guarantee that the server
# process will be initialized and running.
sleep 5;

# Start clients.
for (( i=1; i <= $3; i++ ))
do
	cd "/tmp/Client_"$i;
	./remoteClient -i 127.0.0.1 -d "Server" -p $4 </dev/null &>/dev/null &
done
echo "All clients successfully started!"

# Wait for the execution to finish.
echo "Waiting for the execution to finish..."
sleep 10;
echo "Execution finished!"
echo

for (( i=1; i <= $3; i++ ))
do
	echo "#############################################################################"
	echo "Comparing server's folder with the according folder in Client_"$i": "
	diff -ur "/tmp/Server" "/tmp/Client_"$i"/Server"
	echo "#############################################################################"
	echo
done

#Kill server.
kill -s 9 $serverPID

# Cleanup.
rm -rf /tmp/Server "/tmp/Client_"*

exit 0;
