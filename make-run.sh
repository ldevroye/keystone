trap 'pkill -f qemu-system-riscv64' INT TERM EXIT

./make-examples.sh
echo " "
echo "killing possible last run"
echo " "
pkill -f qemu-system-riscv64 || true
echo " "
echo "starting"
echo " "
./run-rewind.exp