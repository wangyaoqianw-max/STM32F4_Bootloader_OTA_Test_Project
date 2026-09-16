set pagination off
set confirm off

target extended-remote :2331
monitor reset
continue&
echo [S04-PERSIST] continue-and-disconnect complete
disconnect
quit
