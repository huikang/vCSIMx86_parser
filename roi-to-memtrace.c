/*  Created by Hui Kang on 9/3/12.
 *
 *  Convert roi into dineroIV-MC format
 *      D/I coreID address pid(-1) size read(0)/write(1)
 */
#include <stdio.h>
#include "memtrace.h"

#define OUT_FILE_NAME "./output/mem-trace.dat"

static uint64_t vmid; /* 0 means the host machine */

int main(int argc, char *argv[])
{
    FILE *in_file, *out_file;
    char line[LINE_LENGTH];
    int line_nr, i, vmexit_nr, vmrun_nr;
    int event_type;
    char event_name[128], *endptr;
    char vaddr[64], paddr[64], tb_size[64], vm_c[64];
    char vaddr_inc[64], paddr_inc[64];
    char coreid[2];

    if (argc < 2) {
        printf("Usage: ./roi-to-memtrace <filename>\n");
        return 0;
    }
	
    in_file = fopen(argv[1], "r");
    if (!in_file) {
        perror("open input file\n");
        return 1;
    }
	out_file = fopen(OUT_FILE_NAME, "w+");
    if (!out_file) {
        perror("open output file\n");
        return 1;
    }
 
    init_core_mode();

    vmid = 0;
    line_nr = 0;
    vmexit_nr = 0;
    vmrun_nr = 0;
    memset(coreid, 0, 2);
    while (fgets(line, LINE_LENGTH, in_file)) {
        get_field(event_name, line, 0);
        event_type = log_type(event_name);
        switch (event_type) {
        case DECODE:
	    //printf("event: decode at ln %d\n", line_nr);
            get_field(vaddr, line, 1);
            get_field(paddr, line, 2);
            add_to_table(vaddr, paddr);
            break;
	case TRACE:
            get_field(vaddr, line, 1);
            get_field(tb_size, line, 2);
	    get_field(coreid, line, 3);
            //printf("event: trace %s\n", vaddr);
            if (search_tb_table(vaddr, paddr, argv[1], line_nr)) {            
                /* writing consecutive vaddr, paddr of the instruction 
                 * to the file
                 */
                //printf("trace %s tb size: %lu\n",
	        //     vaddr, strtol(tb_size, &endptr, 10));
                for (i = 0; i < strtol(tb_size, &endptr, 10); i++) {
                    increment(vaddr, vaddr_inc, i);
                    increment(paddr, paddr_inc, i);
                    out_to_file(out_file, event_name, vaddr_inc
                                , paddr_inc, coreid, vmid);
                }
            } else {
	      // printf("Can not find corresponding tb paddr\n");
            }
            break;
        case MMU:
            get_field(vaddr, line, 1);
            get_field(paddr, line, 2);
            //printf("vaddr=%s, paddr=%s\n", vaddr, paddr);
            get_field(coreid, line, 3);
            out_to_file(out_file, event_name, vaddr, paddr, coreid, vmid);
            break;
        case VMRUN:
	    /* XXX to be removed  */
            line[strlen(line)] = ',';
	    line[strlen(line)+1] = '\n';
            get_field(vm_c, line, 1);
            get_field(coreid, line, 2);
            //printf("%d vmrun, %s on core %s\n", vmrun_nr, vm_c, coreid);
	    vmid = helper_vmrun(vm_c, coreid);
            vmrun_nr++;
            break;
        case VMEXIT:
            get_field(vm_c, line, 1);
	    get_field(coreid, line, 3);
            //printf("%s", line);
            //printf("%d vmexit,%s on core %s\n", vmexit_nr, vm_c, coreid);
            vmid = helper_vmexit(vm_c, coreid);
            vmexit_nr++;
            break;
        default:
	  //printf("invalide event\n");
            break;
        }
	
        //if (vmexit_nr == 10100050)
	//  break;
		
        if ((line_nr % 155000000) == 0) {
            printf("process line %d\n", line_nr);
	}
        line_nr++;
        //printf("\n");
    }

    printf("Finish! Output file is %s.\n", OUT_FILE_NAME);
    fclose(in_file);
    fclose(out_file);
    return 1;
}
