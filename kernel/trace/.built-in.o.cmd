cmd_kernel/trace/built-in.o :=  /opt/arm-2008q1/bin/arm-none-linux-gnueabi-ld -EL    -r -o kernel/trace/built-in.o kernel/trace/ring_buffer.o kernel/trace/trace.o kernel/trace/trace_nop.o 
