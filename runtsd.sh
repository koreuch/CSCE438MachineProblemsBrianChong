#!/bin/bash


# change this port to whichever you want to use 
#the ip should just be localhost, implemented this locally
ip=localhost

coordinatorPort=3010
masterPort1=50001
masterPort2=50002
masterPort3=50003  
slavePort1=60001
slavePort2=60002
slavePort3=60003
syncPort1=60004
syncPort2=60005
syncPort3=60006


#the ip should just be localhost, implemented this locally
#"p:c:i:I:t:

#echo 'starting the coordinator'
#./coordinator -p $coordinatorPort &
sleep 2
echo 'starting the master servers'
./tsd -c $coordinatorPort -p $masterPort1 -I 0 -t master &
./tsd -c $coordinatorPort -p $masterPort2 -I 1 -t master &
./tsd -c $coordinatorPort -p $masterPort3 -I 2 -t master &
sleep 1
echo 'starting the slave servers'
./tsd -c $coordinatorPort -p $slavePort1 -I 0 -t slave &
./tsd -c $coordinatorPort -p $slavePort2 -I 1 -t slave &
./tsd -c $coordinatorPort -p $slavePort3 -I 2 -t slave &

#echo 'starting the syncers'
#./sync -s $syncPort1 -p $coordinatorPort -i 0 &
#./sync -s $syncPort2 -p $coordinatorPort -i 1 &
#./sync -s $syncPort3 -p $coordinatorPort -i 2 &

