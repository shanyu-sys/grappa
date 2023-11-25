#!/bin/bash

# Step 1: Take Relative .exe Path as Input
exepath=$1 # assuming the first argument is the exe path

# Step 2: Iterate Over Nodes and Copy the Executable
nodes=(1 2 4 5 6 10 11)
for i in "${nodes[@]}"; do
    scp "$exepath" "zion-$i:/mnt/ssd/haoran/types_proj/baseline/grappa/build/Make+Release/${exepath}"
done

# Step 3: Report Finish and Update Done
echo "Update completed for all nodes."
