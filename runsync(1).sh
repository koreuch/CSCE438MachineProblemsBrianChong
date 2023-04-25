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

echo 'starting the syncers'
./sync -s $syncPort1 -p $coordinatorPort -i 0 &
./sync -s $syncPort2 -p $coordinatorPort -i 1 &
./sync -s $syncPort3 -p $coordinatorPort -i 2 &

