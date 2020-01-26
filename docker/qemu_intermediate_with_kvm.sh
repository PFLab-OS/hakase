#!/bin/sh -ex
apk add --no-cache qemu-system-x86_64 qemu-img
cp -r /share/vmlinuz-4.13.0-45-generic /
cp -r /share/rootfs /
mkdir build
cp /share/friend_loader.ko build
apk add rsync
qemu-img create -f qcow2 /backing.qcow2 5G
qemu-system-x86_64 -enable-kvm -cpu Haswell,+vmx,kvm,-kvm-pv-eoi,-kvm-asyncpf,-kvm-steal-time,-kvmclock -s -d cpu_reset -no-reboot -smp 5 -m 4G -D /tmp/qemu.log -hda /backing.qcow2 -kernel /vmlinuz-4.13.0-45-generic -initrd /rootfs -append 'root=/dev/ram rdinit=/sbin/init memmap=0x70000$4K memmap=0x40000000$0x40000000 console=ttyS0,115200' -net nic -net user,hostfwd=tcp::2222-:22 -serial telnet::4444,server,nowait -monitor telnet::4445,server,nowait -nographic -global hpet.msi=true > /dev/null 2>&1 &
while ! sh -c "rsync --list-only toshokan_qemu: > /dev/null 2>&1" ; do
    sleep 1;
done
rsync build/* toshokan_qemu:.
ssh toshokan_qemu sudo insmod friend_loader.ko
echo 'stop' | nc localhost 4445
echo 'savevm snapshot1' | nc localhost 4445
echo 'quit' | nc localhost 4445
cp /backing.qcow2 /share

