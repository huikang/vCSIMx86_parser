#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define KEYWORDS1 "__helper, phys_addr"
#define KEYWORDS2 "_mmu"
#define LINE_LENGTH 100
#define OUT_FILE_NAME "./out.csv"
#define ROI_FILE_NAME "./roi.dat"

#define STARTTAG "invalid"

int main(int argc, char *argv[])
{
    FILE *in_file, *out_file, *roi_file;
    char line[LINE_LENGTH];
    int count, found;
    int section, section_idx;
    int found_section;

    /* open the input log file */
    if (argc < 2) {
	printf("Usage: ./extract_phy_addr <filename> <section>\n");
	return 1;
    }
    if (argc == 3) {
	section = atoi(argv[2]);
    } else {
	section = 0;
    }
    printf("Extract phys addr from %s for section %d\n", argv[1], section);
    
    in_file = fopen(argv[1], "r");
    if (!in_file) {
	perror("open input file\n");
	return 1;
    }
    out_file = fopen(OUT_FILE_NAME, "w+");
    roi_file = fopen(ROI_FILE_NAME, "w+");

    /* extrace the line with physical address */
    count = 0;
    found = 0;
    section_idx = 0;
    found_section = 0;
    section = section * 2;
    while(fgets(line, LINE_LENGTH, in_file)) {
        char out_line[LINE_LENGTH];
        char *start;
	
        start = strstr(line, STARTTAG);
        if (start) {
            printf("%s", line);
            
            if (section_idx == section) {
                found = 1;
            }
            else if (section_idx == (section + 1)) {
                break;
            }
            section_idx++;
        }
	
        if (found) {
            fputs(line, roi_file);

            start = strstr(line, KEYWORDS1);
            if (start) {
                //printf("%s", line);
                strcpy(out_line, line + 13);
                /* append , for csv output */
                out_line[strlen(out_line)-2] = ',';
                //fputs(line, out_file);
                /* for debug 
                 count++;
                 if (count > 20)
                 break;*/
                }
            else {
                start = strstr(line, KEYWORDS2);
	      	    if (start) {
                    //printf("%s", line); 
                    strcpy(out_line, line + 13);
                    /* append , for csv output */
                    out_line[strlen(out_line)-2] = ',';
                    //fputs(line, out_file);
                    /* for debug 
                    count++;
                    if (count > 20)
                    break;*/
                }
            }
        }
    }
    
    fclose(in_file);
    fclose(out_file);
    fclose(roi_file);
    return 0;
}
