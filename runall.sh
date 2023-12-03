cd ~/types_proj/baseline/grappa/build/Make+Release
# mpirun -np 80 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran5 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.6 > newresult5.txt
# mpirun -np 64 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran4 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.6 > newresult4.txt
# mpirun -np 48 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran3 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.6 > newresult3.txt
mpirun -np 16 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran1 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.7 > newresult1.txt
mpirun -np 32 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran2 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.7 > newresult2.txt
# mpirun -np 96 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran7 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.6 > result6.txt
# mpirun -np 112 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran7 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.5 > result7.txt
# mpirun -np 128 --hostfile ~/types_proj/baseline/grappa/hostsfile_haoran7 applications/dataframe/dataframe --locale_shared_fraction 0.8 --global_heap_fraction 0.5 > result8.txt