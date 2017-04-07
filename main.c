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
int last = 0;
int nfaults = 0, nwrites = 0, nreads = 0;

void page_fault_handler( struct page_table *pt, int page )
{
	++nfaults;
	//printf("page fault on page #%d\n",page);

	int i;
	int nframes = page_table_get_nframes(pt);
	int frame;
	int bits;
	page_table_get_entry(pt, page, &frame, &bits);
	char *physmem = page_table_get_physmem(pt);
	//printf("page %d located at frame %d\n", page, frame);

	if(bits == 0)	//this page is not present, so load it
	{
		//printf("loading in new page %d\n", page);
		for(i = 0; i < nframes; i++)  //check for empty frames
		{
			if(table[i] == -1)
			{
			//	printf("found empty frame at %d\n", i);
				page_table_set_entry(pt, page, i, PROT_READ);
				disk_read(disk, page, &physmem[i*PAGE_SIZE]);
				++nreads;
				table[i] = page;
				return;
			}
		}

		//if there are no empty frames, we will pick
		//a frame to replace. What frame we pick
		//is determined by the specified algorithm.  
		//If that frame has been written to, we need to write 
		//those changes to disk
		if(!strcmp("rand", algorithm))
		{
			int r = rand() % nframes;	//frame # to replace
			page_table_get_entry(pt, page, &frame, &bits);
			if (bits != PROT_READ) 
			{
				//printf("replacing frame %d and writing to disk\n", r);
				disk_write(disk, table[r], &physmem[r*PAGE_SIZE]);
				++nwrites;
				//write changes to disk, now ready to replace
			}
			disk_read(disk, page, &physmem[r*PAGE_SIZE]);
			++nreads;
			page_table_set_entry(pt, page, r, PROT_READ);
			page_table_set_entry(pt, table[r], 0, 0);
			//update page table and remove reference to old page

			table[r] = page;

		}
		else if(!strcmp("fifo", algorithm))
		{
			page_table_get_entry(pt, page, &frame, &bits);
			if (bits != PROT_READ)
			{
				//printf("replacing frame %d and writing to disk\n", last);
				disk_write(disk, table[last], &physmem[last*PAGE_SIZE]);
				++nwrites;
				//write changes to disk, now ready to replace
			}
			disk_read(disk, page, &physmem[last*PAGE_SIZE]);
			++nreads;
			page_table_set_entry(pt, page, last, PROT_READ);
			page_table_set_entry(pt, table[last], 0, 0);
			//update page table and remove reference to old page

			table[last] = page;
			last = (last + 1) % nframes;
		}
		else if(!strcmp("custom", algorithm)) {}
	}
	else if(bits != 0)	//this is trying to write and needs perms
	{
		//printf("Adding write bit to frame %d\n", frame);
		//the relevant data is already loaded
		//it just needs write access
		page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
	}
	
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

	printf("Program ran with %d page faults, %d disk reads, and %d disk writes\n", nfaults, nreads, nwrites);

	return 0;
}
