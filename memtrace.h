//
//  memtrace.h
//  
//
//  Created by Hui Kang on 9/3/12.
//
//

#ifndef _memtrace_h
#define _memtrace_h

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#ifdef __GNUC__
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#define LINE_LENGTH 256

#define MAX_CORE_NR 8
#define MAX_VM_NR 8

// XXX fix it, refer to qemu cpu-defs.h
#define TARGET_PAGE_BITS  12

#define TB_JMP_CACHE_BITS 12
#define TB_JMP_CACHE_SIZE (1 << TB_JMP_CACHE_BITS)
#define TB_JMP_PAGE_BITS (TB_JMP_CACHE_BITS / 2)
#define TB_JMP_PAGE_SIZE (1 << TB_JMP_PAGE_BITS)
#define TB_JMP_ADDR_MASK (TB_JMP_PAGE_SIZE - 1)
#define TB_JMP_PAGE_MASK (TB_JMP_CACHE_SIZE - TB_JMP_PAGE_SIZE)

#define NUM_EVENTS 5
enum {
	DECODE = 0,
	TRACE,
	MMU,
	VMRUN,
	VMEXIT
};

extern char *event_names[NUM_EVENTS];

int get_field(char *field, char *line, int idx);
int log_type(char *name);
void init_core_mode();

int add_to_table(const char vaddr[], const char paddr[]);
int search_tb_table(const char vaddr[], char paddr[], char *in_filename, int ln);
void increment(const char addr[], char addr_inc[], int addend);

uint64_t helper_vmexit(const char vm_c[], const char core_id_s[]);
uint64_t helper_vmrun(const char vm_c[], const char core_id_s[]);

int out_to_file(FILE *out_file, char event_name[], char vaddr[], char paddr[],
                const char coreid[], uint64_t vmid);

static inline unsigned int table_hash_func(uint64_t addr)
{
    uint64_t tmp;
    tmp = addr ^ (addr >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS));
    return (((tmp >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS)) & TB_JMP_PAGE_MASK)
            | (tmp & TB_JMP_ADDR_MASK));
}


#endif
