set pagination off
set confirm off
set print pretty on

target remote localhost:2331

echo ===== GDB RUNTIME SNAPSHOT =====\n

echo \n[REGISTERS]\n
info registers

echo \n[CORE]\n
printf "PC = 0x%08x\n", $pc
printf "SP = 0x%08x\n", $sp
printf "LR = 0x%08x\n", $lr
printf "XPSR = 0x%08x\n", $xpsr

echo \n[SOURCE]\n
info line *$pc

echo \n[BACKTRACE]\n
bt

echo \n[STACK]\n
x/16wx $sp

echo \n[EXIT]\n
echo resume-and-disconnect\n
continue&
disconnect
quit
