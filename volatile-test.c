
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

#define SYS_mvolatile 316

#define MVOLATILE_NONVOLATILE 0
#define MVOLATILE_VOLATILE 1


static ssize_t mvolatile(unsigned long start, size_t length,
		unsigned long mode, unsigned long flags, int *purged)
{
	return syscall(SYS_mvolatile, start, length, mode, flags, purged);
}


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
	int i,j, purged;
	char* file = NULL;
	int fd;
	int pressure = 0;
	int opt;
	int signal = 0;
	char* vaddr;

	/* Process arguments */
	while ((opt = getopt(argc, argv, "p:f:s"))!=-1) {
		switch(opt) {
		case 'p':
			pressure = atoi(optarg);
			break;
		case 'f':
			file = optarg;
			break;
		case 's':
			signal = 1;
			break;
		default:
			printf("Usage: %s [-p <mempressure in megs>] [-f <filename>] -s\n", argv[0]);
			printf("        -p: Amount of memory pressure to generate\n");
			printf("        -f: Use a file\n");
			printf("	-s: Traverse volatile data (trigger SIGBUS)\n");
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
		ssize_t ret;
		ret = mvolatile((long)(vaddr + (i*CHUNK)), CHUNK, MVOLATILE_VOLATILE, 0, 0);
		if (ret != CHUNK)
			printf("Did not set full chunk volatile!\n");
		i+=2;
	}
	printf("Generating %i megs of pressure\n", pressure);
	generate_pressure(pressure);

	/* try to force sig-bus */
	if (signal) {
		for(i=0; i < CHUNKNUM; i++) {
			for (j=0; j < CHUNK/PAGE_SIZE; j++)
				printf("%c", vaddr[i*CHUNK+j*PAGE_SIZE]);
			printf("\n");
		}
	}

	purged = 0;
	for(i=0; i < CHUNKNUM; ) {
		int tmp_purged = 0;
		ssize_t ret;
		ret = mvolatile((long)(vaddr + (i*CHUNK)), CHUNK, MVOLATILE_NONVOLATILE, 0,
				&tmp_purged);
		if (ret != CHUNK)
			printf("Did not set full chunk non-volatile!\n");

		purged |= tmp_purged;
		i+=2;
	}

	if (purged)
		printf("Data purged!\n");

	for(i=0; i < CHUNKNUM; i++) {
		for (j=0; j < CHUNK/PAGE_SIZE; j++)
			printf("%c", vaddr[i*CHUNK+j*PAGE_SIZE]);
		printf("\n");
	}
	


	return 0;
}

