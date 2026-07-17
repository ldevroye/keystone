./make-examples.sh;
echo " ";
echo "killing possible last run";
echo " ";
pkill -f qemu-system-riscv64;
echo " ";
echo "starting";
echo " ";
./run-rewind.exp;