./rpc.py construct_aio_bdev /dev/rdx_dimec AIO_0 4096
./rpc.py construct_lvol_store AIO_0 lvs
./rpc.py construct_lvol_bdev lvol1 1000000 -l lvs

./rpc.py add_portal_group 1 172.16.21.75:3261
./rpc.py add_initiator_group 2 ANY 172.16.21.0/24

./rpc.py construct_target_node Target1 Target1_alias lvs/lvol1:0 1:2 128

#iscsiadm -m discovery -t sendtargets -p 172.16.21.75:3261
#iscsiadm --mode node --targetname iqn.2016-06.io.spdk:Target1 --portal 172.16.21.75:3261 --login
~                                                                                                                                       
~                                                                                                                                       
~                                                                                                                                       
~                                                                                                                                       
~                                                                                                                                       
~                                                                                                                                       
~                                                                                                                                       
~                                                                                                             
