CC=gcc
CFLAGS=-c -Wall

all: extract_roi_qemu_mem_trace roi-to-memtrace

extract_roi_qemu_mem_trace: extract_roi_qemu_mem_trace.o
	${CC} extract_roi_qemu_mem_trace.o -o extract_roi_qemu_mem_trace
extract_roi_qemu_mem_trace.o: extract_roi_qemu_mem_trace.c
	${CC} ${CFLAGS} extract_roi_qemu_mem_trace.c


helper.o: helper.c
	${CC} ${CFLAGS} helper.c
roi-to-memtrace.o: roi-to-memtrace.c
	${CC} ${CFLAGS} roi-to-memtrace.c
roi-to-memtrace: helper.o roi-to-memtrace.o
	${CC}  helper.o roi-to-memtrace.o -o roi-to-memtrace

clean:
	rm -rf *.o extract_roi_qemu_mem_trace roi-to-memtrace

