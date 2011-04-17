cmd_ipc/built-in.o :=  /opt/arm-2008q1/bin/arm-none-linux-gnueabi-ld -EL    -r -o ipc/built-in.o ipc/util.o ipc/msgutil.o ipc/msg.o ipc/sem.o ipc/shm.o ipc/ipcns_notifier.o ipc/ipc_sysctl.o 
