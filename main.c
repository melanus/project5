/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <time.h>

char *algorithm = "rand";
int *table = NULL;
struct disk *disk;

void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);
	if(!strcmp("rand", algorithm))
	{
		//printf("started rand\n");
		int i;
		int nframes = page_table_get_nframes(pt);
		int frame;
		int bits;
		page_table_get_entry(pt, page, &frame, &bits);
		char *physmem = page_table_get_physmem(pt);

		if(bits == 0)	//this page is not present, so load it
		{
			for(i = 0; i < nframes; i++)  //check for empty frames
			{
				if(table[i] == -1)
				{
					page_table_set_entry(pt, page, i, PROT_READ);
					disk_read(disk, page, &physmem[i*PAGE_SIZE]);
					table[i] = page;
					return;
				}
			}

			//if there are no empty frames, we will pick
			//a frame at random to replace. If that frame
			//has been written to, we need to write 
			//those changes to disk

			int r = rand() % nframes;	//frame # to replace
			page_table_get_entry(pt, page, &frame, &bits);
			if (bits >= 3)  //corrsponds to 011, write bit set
			{
				disk_write(disk, table[r], &physmem[r*PAGE_SIZE]);
				//write changes to disk, now ready to replace
			}
			disk_read(disk, page, &physmem[r*PAGE_SIZE]);
			page_table_set_entry(pt, page, r, PROT_READ);
			page_table_set_entry(pt, table[r], 0, 0);
			//update page table and remove reference to old page

			table[r] = page;

		}
		else if(bits != 0)	//this is trying to write and needs perms
		{
			//the relevant data is already loaded
			//it just needs write access
			page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
		}
	}

	//test value
	//page_table_set_entry(pt, page, page, PROT_READ|PROT_WRITE);
	//exit(1);
}

int main( int argc, char *argv[] )
{
	srand(time(NULL));

	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	algorithm = argv[3];
	const char *program = argv[4];

	//resize table to be number of frames
	table = realloc(table, nframes * sizeof(int));
	int i;
	for (i = 0; i < nframes; i++)
	{
		table[i] = -1;  //-1 indicates empty
	}

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	char *physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
		return 1;
	}

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
