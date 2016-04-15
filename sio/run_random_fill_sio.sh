#!/bin/sh
if [ $# -ne 1 ]
then
   echo "Specify output dir"
   exit 1
fi
bs=16k
fsize=5g
dur=120
thd=1

out_dir=$1
os=`uname`

if [ $os == "Linux" ]
then
   drv=./sio_ntap_linux
   dev=/dev/sdb
else if [ $os == "FreeBSD" ]
     then
        drv=./sio_ntap_freebsd
        dev=/dev/da1
     fi
fi

if [ ! -d $out_dir ]
then
   mkdir $out_dir
fi
$drv 0 0 $bs $fsize $dur $thd $dev -direct -struct_output -write_until_end -random_fill > $out_dir/seqW.txt
$drv 100 0 $bs $fsize $dur $thd $dev -direct -struct_output > $out_dir/seqR.txt
$drv 100 100 $bs $fsize $dur $thd $dev -direct -struct_output > $out_dir/ranR.txt
$drv 0 100 $bs $fsize $dur $thd $dev -direct -struct_output -random_fill > $out_dir/ranW.txt
$drv 50 50 $bs $fsize $dur $thd $dev -direct -struct_output -random_fill > $out_dir/seqranRW.txt
