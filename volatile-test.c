
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

#define SYS_vrange 316

#define VRANGE_NOVOLATILE 0	/* pin all pages so VM can't discard them */
#define VRANGE_VOLATILE	1	/* unpin all pages so VM can discard them */

#define VRANGE_MODE_SHARED 0x1	/* discard all pages of the range */



#define VRANGE_MODE 0x1

static ssize_t vrange(unsigned long start, size_t length,
		unsigned long mode, unsigned long flags, int *purged)
{
	return syscall(SYS_vrange, start, length, mode, flags, purged);
}


static ssize_t mvolatile(void *addr, size_t length)
{
	return vrange((long)addr, length, VRANGE_VOLATILE, 0, 0);
}


static ssize_t mnovolatile(void *addr, size_t length, int* purged)
{
	return vrange((long)addr, length, VRANGE_NOVOLATILE, 0, purged);
}


char* vaddr;
#define PAGE_SIZE (4*1024)
#define CHUNK (PAGE_SIZE*16)
#define CHUNKNUM 26
#define FULLSIZE (CHUNK*CHUNKNUM + 2*PAGE_SIZE)

void generate_pressure(megs)
{
	pid_t child;
	int one_meg = 1024*1024;
	char *addr;
	int i, status;

	child = fork();
	if (!child) {
		for (i=0; i < megs; i++) {
			addr = malloc(one_meg);
			bzero(addr, one_meg);		
		}
		exit(0);
	}

	waitpid(child, &status, 0);
	return;
}

int main(int argc, char *argv[])
{
	int i, purged;
	char* file = NULL;
	int fd;
	int pressure = 0;
	int opt;

        /* Process arguments */
        while ((opt = getopt(argc, argv, "p:f:"))!=-1) {
                switch(opt) {
                case 'p':
                        pressure = atoi(optarg);
                        break;
                case 'f':
                        file = optarg;
                        break;
                default:
                        printf("Usage: %s [-p <mempressure in megs>] [-f <filename>]\n", argv[0]);
                        printf("        -p: Amount of memory pressure to generate\n");
                        printf("        -f: Use a file\n");
                        exit(-1);
                }
        }

	if (file) {
		printf("Using file %s\n", file);
		fd = open(file, O_RDWR);
		vaddr = mmap(0, FULLSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	} else {
		vaddr = malloc(FULLSIZE);
	}

	vaddr += PAGE_SIZE-1;
	vaddr -= (long)vaddr % PAGE_SIZE;

	for(i=0; i < CHUNKNUM; i++)
		memset(vaddr + (i*CHUNK), 'A'+i, CHUNK);


	for(i=0; i < CHUNKNUM; ) {
		mvolatile(vaddr + (i*CHUNK), CHUNK);
		i+=2;
	}

	printf("Generating %i megs of pressure\n", pressure);
	generate_pressure(pressure);

	purged = 0;
	for(i=0; i < CHUNKNUM; ) {
		int tmp_purged = 0;
		mnovolatile(vaddr + (i*CHUNK), CHUNK, &tmp_purged);
		purged |= tmp_purged;
		i+=2;
	}

	if (purged)
		printf("Data purged!\n");

	for(i=0; i < CHUNKNUM; i++)
		printf("%c\n", vaddr[i*CHUNK]);
	


	return 0;
}

