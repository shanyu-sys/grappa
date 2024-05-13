num=$1
echo "Preparing workload KV for Grappa"

echo $num

if [ $num -eq 8 ]; then    
    echo "8 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_1.csv grappadht/data.csv'
    ssh guest@zion-3.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_2.csv grappadht/data.csv'
    ssh guest@zion-4.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_3.csv grappadht/data.csv'
    ssh guest@zion-5.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_4.csv grappadht/data.csv'
    ssh guest@zion-6.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_5.csv grappadht/data.csv'
    ssh guest@zion-10.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_6.csv grappadht/data.csv'
    ssh guest@zion-11.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_8_7.csv grappadht/data.csv'
elif [ $num -eq 7 ]; then
    echo "7 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_1.csv grappadht/data.csv'
    ssh guest@zion-3.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_2.csv grappadht/data.csv'
    ssh guest@zion-4.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_3.csv grappadht/data.csv'
    ssh guest@zion-5.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_4.csv grappadht/data.csv'
    ssh guest@zion-6.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_5.csv grappadht/data.csv'
    ssh guest@zion-10.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_7_6.csv grappadht/data.csv'
elif [ $num -eq 6 ]; then
    echo "6 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_6_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_6_1.csv grappadht/data.csv'
    ssh guest@zion-3.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_6_2.csv grappadht/data.csv'
    ssh guest@zion-4.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_6_3.csv grappadht/data.csv'
    ssh guest@zion-5.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_6_4.csv grappadht/data.csv'
    ssh guest@zion-6.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_6_5.csv grappadht/data.csv'
elif [ $num -eq 5 ]; then
    echo "5 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_5_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_5_1.csv grappadht/data.csv'
    ssh guest@zion-3.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_5_2.csv grappadht/data.csv'
    ssh guest@zion-4.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_5_3.csv grappadht/data.csv'
    ssh guest@zion-5.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_5_4.csv grappadht/data.csv'
elif [ $num -eq 4 ]; then
    echo "4 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_4_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_4_1.csv grappadht/data.csv'
    ssh guest@zion-3.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_4_2.csv grappadht/data.csv'
    ssh guest@zion-4.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_4_3.csv grappadht/data.csv'
elif [ $num -eq 3 ]; then
    echo "3 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_3_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_3_1.csv grappadht/data.csv'
    ssh guest@zion-3.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_3_2.csv grappadht/data.csv'
elif [ $num -eq 2 ]; then
    echo "2 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_2_0.csv grappadht/data.csv'
    ssh guest@zion-2.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_2_1.csv grappadht/data.csv'
elif [ $num -eq 1 ]; then
    echo "1 servers"
    ssh guest@zion-1.cs.ucla.edu 'cd ~/DRust_home/dataset; mkdir -p grappadht; cp dht/zipf/gam_data_0.99_100000000_1_0.csv grappadht/data.csv'
fi
echo "Done"