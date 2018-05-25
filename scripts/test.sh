scripts/rpc.py construct_aio_bdev /dev/rdx_dimec AIO_0 4096
scripts/rpc.py construct_lvol_store AIO_0 lvs
scripts/rpc.py construct_lvol_bdev lvol1 1000000 -l lvs
#scripts/rpc.py add_initiator_group 2 ANY 127.0.0.1/32
#scripts/rpc.py add_portal_group 1 127.0.0.1:3260
#scripts/rpc.py construct_target_node Target1 Target1_alias lvs/lvol1:0 1:2 128

#./rpc.py construct_nvmf_subsystem nqn.2018-05.io.rdx:raidix4 'trtype:RDMA traddr:10.30.0.4 trsvcid:4420' nqn.2016-07.io.spdk:init --allow-any-host -n 'lvs/lvol1:1'

#./rpc.py construct_nvme_bdev -b Nvme0 -t pcie -a 0000:06:00.0

