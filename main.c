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
int last = 0;

void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);
	if(strcmp("rand", algorithm))
	{
		int i;
		int nframes = page_table_get_nframes(pt);
		int frame;
		int bits;
		page_table_get_entry(pt, page, &frame, &bits);

		if(bits == 0)	//this is loading into empty frame
		{
			for(i = 0; i < nframes; i++)  //check for empty frames
			{
				if(table[i] == -1)
				{
					page_table_set_entry(pt, page, i, PROT_READ);
					table[i] = page;
					return;
				}
			}
		}
		else if(bits == 3)	//this is trying to write and needs perms
		{
			page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
		}
		else if(bits == 7)
		{
			//write to disk
		}
	}
	else if(strcmp("fifo", algorithm))
	{
		int i;
		int nframes = page_table_get_nframes(pt);
		int frame;
		int bits;
		page_table_get_entry(pt, page, &frame, &bits);

		if(bits == 0)	//this is loading into empty frame
		{
			page_table_set_entry(pt, page, last, PROT_READ);

			// find the frame number
			last++;
			last = last % nframes; // don't want to exceed # of frames
		}
		else if(bits == 3)	//this is trying to write and needs perms
		{
			page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
		}
		else if(bits == 7)
		{
			// Open disk and write to disk
			disk_write(disk, block, data);
			
			// add new entry to page table
			page_table_set_entry(pt, page, last, PROT_READ);
			//write to disk
		}
	}
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

	struct disk *disk = disk_open("myvirtualdisk",npages);
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
