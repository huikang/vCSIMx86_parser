//
//  helper.c
//  
//
//  Created by Hui Kang on 9/3/12.
//
//

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "memtrace.h"

typedef struct tb_trans_entry {
    //char vaddr;
    //char paddr;
    uint64_t vaddr;
    char paddr[64];
} tb_trans_entry;

tb_trans_entry tb_trans_table[TB_JMP_CACHE_SIZE];

/* Must match the trace events */
char *event_names[NUM_EVENTS] = {
    "decode",
    "trace",
    "_mmu",
    "vmrun",
    "vmexit",
};

#define NUM_VM 2
int sp = 0;
uint64_t vm_stack[NUM_VM];
int fault_allowed = NUM_VM;
void push(uint64_t vmid);
uint64_t pop(void);
uint64_t top(void);

int log_type(char *name)
{
    int j;
    j = 0;
    for (j = 0; j < NUM_EVENTS; j++) {
        if (strstr(name, event_names[j])) {
            //printf("event: %s\n", event_names[j]);
            return j;
        }
    }
    return -1;
}

/* Return the idx field of the line. The fields are deliminated by comma(,)
 */
int get_field(char field[], char *line, int idx)
{
    char *pos, *pre_pos;
    int len;
    int j;
    int field_id;

    //printf("Get %dth from: %s", idx, line);
    field_id = 0;
    j = 0;
    pos = pre_pos = line;
    
    while ((pos = strchr(pre_pos, ','))) {
	//printf("pos=%p\n", pre_pos);
        if (field_id == idx) {
	    //printf("Found %d field\n", idx);
            len = pos - pre_pos;
            //printf("len %d\n", len);
            strncpy(field, pre_pos, len);
            field[len] = '\0';
            //printf("field is %s\n", field);
            break;
        }
	
        pre_pos = pos + 1;
        field_id++;
    }
	
    return -1;
}

/* Add a decoded vaddr and paddr into the table.
 */
int add_to_table(const char vaddr[], const char paddr[])
{
    int errno;
    char *endptr;
    uint64_t addr;
    tb_trans_entry *te;

    // convert vaddr to hex integer
    addr = strtoul(vaddr, &endptr, 16);
    if ((errno == ERANGE && (addr == LONG_MAX || addr == LONG_MIN))
        || (errno != 0 && addr == 0)) {
        perror("strtoul");
        exit(EXIT_FAILURE);
    }

    te = &tb_trans_table[table_hash_func(addr)];
    tb_trans_table[table_hash_func(addr)].vaddr = addr;
    strcpy(tb_trans_table[table_hash_func(addr)].paddr, paddr);
    //printf("Fill table: vaddr=0x%s, addr=%016" PRIx64 ", paddr=%s\n",
    //       vaddr, addr, tb_trans_table[table_hash_func(addr)].paddr);

    return 1;
}

int tb_paddr_find_slow(const char vaddr[], char paddr[], char *in_filename, int ln_max)
{
    int found;
    FILE *in_file;
    char line[LINE_LENGTH];
    int line_nr;
    char tmp_vaddr[64];
    int event_type;
    char event_name[128];
    
    //printf("%s search for %s PC\n", __func__, vaddr);
    in_file = fopen(in_filename, "r");
    if (!in_file) {
        perror("open input file\n");
        return 1;
    }
    
    line_nr = 0;
    found = 0;
    while (fgets(line, LINE_LENGTH, in_file) && line_nr <= ln_max) {
        get_field(event_name, line, 0);
        event_type = log_type(event_name);
        if (event_type == DECODE) {
            get_field(tmp_vaddr, line, 1);
            if (strcmp(tmp_vaddr, vaddr) == 0) {
                found = 1;
                get_field(paddr, line, 2);
                break;
            }
        }
        line_nr++;
    }
    
    fclose(in_file);
    return found;
}

/* Given a vaddr, search the tb table for paddr. And write results to paddr
 */
int search_tb_table(const char vaddr[], char paddr[], char *in_filename, int ln)
{
    uint64_t addr;
    char *endptr;
    tb_trans_entry *te;
    int found; 
    
    // convert vaddr to hex integer
    addr = strtoull(vaddr, &endptr, 16);
    if ((errno == ERANGE && (addr == LONG_MAX || addr == LONG_MIN))
        || (errno != 0 && addr == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    //printf("%s, vaddr=0x%s, vaddr=%llu\n", __func__, vaddr, addr);
    
    te = &tb_trans_table[table_hash_func(addr)];
    if (unlikely(te->vaddr != addr)) {
      //printf("%s, vaddr=0x%s vaddr=%016" PRIx64 "at %u not in the table\n",
      //       __func__, vaddr, addr, table_hash_func(addr));

        /* XXX slow path: search and add to tb_table */
        // found = tb_paddr_find_slow(vaddr, paddr, in_filename, ln);
        found = 0;
        if (found) {
            tb_trans_table[table_hash_func(addr)].vaddr = addr;
            strcpy(tb_trans_table[table_hash_func(addr)].paddr, paddr);
            printf("Add paddr=%s\n", tb_trans_table[table_hash_func(addr)].paddr);
        } else {
            return 0;
        }
    }

    strcpy(paddr, tb_trans_table[table_hash_func(addr)].paddr);
    // printf("Found paddr=%s\n", tb_trans_table[table_hash_func(addr)].paddr);
    return 1;
}

/* convert addr to long integer and incremented by addend, return the results 
 * in vaddr_inc
 */
void increment(const char addr[], char addr_inc[], int addend)
{
    uint64_t value;
    char *endptr;
    
    // convert vaddr to hex integer
    value = strtoull(addr, &endptr, 16);
    if ((errno == ERANGE && (value == LONG_MAX || value == LONG_MIN))
        || (errno != 0 && value == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    
    value += addend;
    sprintf(addr_inc, "%016" PRIx64, value);
    //printf("%s %d %s\n", addr, addend, addr_inc);
}

/* core mode could be invalid(-1), hypervisor(0), or vmcb addr */
uint64_t core_mode[MAX_CORE_NR];

/* vm_index could be invalid(-1), vmcb addr.
 * we will use the index to encode the vmid, instead of vmcb addr
 */
uint64_t vm_index[MAX_VM_NR];

void init_core_mode()
{
    int i;
    for(i = 0; i < MAX_CORE_NR; i++) {
        core_mode[i] = -1;
    }

    for(i = 0; i < MAX_VM_NR; i++) {
        vm_index[i] = -1;
    }
    // index 0 is for hypervisor
    vm_index[0] = 0;
}

void insert_vmgroup(uint64_t vmid) {
    int i;
    
    i = 0;
    for (i = 0; i < MAX_VM_NR; i++) {
        if (vmid == vm_index[i]) {
            return;
	} else if (vm_index[i] == -1) {
            vm_index[i] = vmid;
            return;
	}
    }
}

uint64_t helper_vmrun(const char vm_c[], const char core_id_s[])
{
    uint64_t vmid;
    char *endptr;
    int core_id;

    vmid = strtoull(vm_c, &endptr, 16);
    if ((errno == ERANGE && (vmid == LONG_MAX || vmid == LONG_MIN))
        || (errno != 0 && vmid == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    //printf("%s %016" PRIx64 "\n", __func__, vmid);

    core_id = strtoul(core_id_s, &endptr, 10);
    if ((errno == ERANGE && (core_id == LONG_MAX || core_id == LONG_MIN))
        || (errno != 0 && core_id == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    //printf("%s coreid %d\n", __func__, core_id);

    assert(core_mode[core_id] == 0 || core_mode[core_id] == -1);
    core_mode[core_id] = vmid;

    insert_vmgroup(vmid);

    return vmid;
}

uint64_t helper_vmexit(const char vm_c[], const char core_id_s[])
{
    uint64_t vmid;
    char *endptr;
    int core_id;

    vmid = strtoull(vm_c, &endptr, 16);
    if ((errno == ERANGE && (vmid == LONG_MAX || vmid == LONG_MIN))
        || (errno != 0 && vmid == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    //printf("%s %016" PRIx64 "\n", __func__, vmid);

    core_id = strtoul(core_id_s, &endptr, 10);
    if ((errno == ERANGE && (core_id == LONG_MAX || core_id == LONG_MIN))
        || (errno != 0 && core_id == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    //printf("%s coreid %d\n", __func__, core_id);
    
    if (core_mode[core_id] == -1) {
        //printf("vmexit first time\n");
        core_mode[core_id] = 0;
        return vmid;
    }
    assert(core_mode[core_id] == vmid);
    core_mode[core_id] = 0;

    insert_vmgroup(vmid);

    return vmid;
}

int out_to_file(FILE *out_file, char event_name[], char vaddr[], char paddr[],
                const char coreid[], uint64_t vmid_l)
{
    char type;
    char line[128];
    char data_size;
    char event;
    int i;
    int coreid_t, vmid;
    char *endptr;

    if (strstr(event_name, "_mmu")) {
        event = 'D';
    } else if (strstr(event_name, "trace")) {
	event = 'I';
    }

    /* determine the acces type of the request
     * l/s/I
     */
    if (strstr(event_name, "ld")) {
        type = '0';
    } else if (strstr(event_name, "st")) {
        type = '1';
    } else {
        type = ' ';
    }

    if (strstr(event_name, "b_")) {
        data_size = '3'; /* 2^3 */
    } else if (strstr(event_name, "w_")) {
        data_size = '4'; /* 2^4 */
    } else if (strstr(event_name, "l_")) {
        data_size = '5';
    } else if (strstr(event_name, "q_")) {
        data_size = '6';
    } else {
        data_size = '3'; /* instruction */
    }

    coreid_t = strtoull(coreid, &endptr, 10);
    if ((errno == ERANGE && (coreid_t == LONG_MAX || coreid_t == LONG_MIN))
        || (errno != 0 && coreid_t == 0)) {
        perror("strtoull");
        exit(EXIT_FAILURE);
    }
    //printf("%s coreid=%d\n", __func__, coreid_t);

    /* core in hypervisor or initial status */
    vmid = 0;
    if (core_mode[coreid_t] != 0 && core_mode[coreid_t] != -1) {
        i = 0;
        for(i = 0; i < MAX_VM_NR; i++) {
            if (core_mode[coreid_t] == vm_index[i]) {
                vmid = i;
                break;
	    }
        }
    }
    //printf("vid=%d %s\n", vmid, paddr);

    //sprintf(line, "%c,%s,%s,%016" PRIx64 ",\n", type, vaddr, paddr, vmid);
    sprintf(line, "%c %s %s %d %c %c %d\n"
            , event, coreid, paddr, -1, data_size, type, vmid);

    if (strstr(line, "ffffffff")) {
      printf("%s", line);
      return 1;
    }
    
    fputs(line, out_file);

    return 1;
}

