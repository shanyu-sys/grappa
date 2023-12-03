ratio=90

mpirun -np 8 --host zion-4,zion-3,zion-2,zion-1,zion-5,zion-6,zion-10,zion-11  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_8.txt
echo "8 Nodes Done"
mpirun -np 7 --host zion-4,zion-3,zion-2,zion-1,zion-5,zion-6,zion-11  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_7.txt
echo "7 Nodes Done"
mpirun -np 6 --host zion-4,zion-3,zion-2,zion-1,zion-5,zion-6  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_6.txt
echo "6 Nodes Done"
mpirun -np 5 --host zion-4,zion-3,zion-5,zion-6,zion-10  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_5.txt
echo "5 Nodes Done"
mpirun -np 4 --host zion-4,zion-3,zion-5,zion-6  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_4.txt
echo "4 Nodes Done"
mpirun -np 3 --host zion-4,zion-3,zion-5  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_3.txt
echo "3 Nodes Done"
mpirun -np 2 --host zion-4,zion-3  applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_2.txt
echo "2 Nodes Done"
mpirun -np 1 applications/kvstore/kvstore --get_ratio $ratio > logs/kvstore/ratio_${ratio}_1.txt
echo "1 Nodes Done"