
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <signal.h>

#define SYS_vrange 316

#define VRANGE_VOLATILE	1	/* unpin all pages so VM can discard them */
#define VRANGE_NOVOLATILE	0	/* pin all pages so VM can't discard them */

#define VRANGE_MODE_SHARED 0x1	/* discard all pages of the range */



#define VRANGE_MODE 0x1

static ssize_t vrange(unsigned long start, size_t length, unsigned long mode,
			unsigned long flags, int *purged)
{
	return syscall(SYS_vrange, start, length, mode, flags, purged);
}


static int mvolatile(void *addr, size_t length)
{
	return vrange((long)addr, length, VRANGE_VOLATILE, 0, 0);
}


static int mnovolatile(void *addr, size_t length, int* purged)
{
	return vrange((long)addr, length, VRANGE_NOVOLATILE, 0, purged);
}


char* vaddr;
#define PAGE_SIZE (4*1024)
#define CHUNK (4*1024*4)
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

void sigaction_sigbusy(int signum, siginfo_t *info, void *ctxt)
{
	char *ptr;
	int ret;
	char x;
	long len;
	if (signum != SIGBUS)
		return;

	ptr = info->si_addr;

	mnovolatile(ptr, CHUNK, &ret);
	printf("Fixing up data\n");
	len = (ptr - vaddr)/CHUNK;
	x = 'A' + len; 
	memset(ptr, x, CHUNK);
	printf("%c\n", x);
	
}

void signal_handler_sigbusy(int signum)
{
	if (signum == SIGBUS) {
		printf("We received SIGBUSY\n");
	}
}

void register_signal_handler()
{
	struct sigaction action;
	action.sa_sigaction = &sigaction_sigbusy;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO;
	action.sa_restorer = NULL;
	sigaction(SIGBUS, &action, NULL);
}

int main(int argc, char *argv[])
{
	int i, purged;
	char* file = NULL;
	int fd;
	int pressure = 0;
	int opt;

	//signal(SIGBUS, signal_handler_sigbusy
	//sigaction(SIGBUS, sigaction_sigbusy, NULL);
	register_signal_handler();

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
		file = argv[1];
		fd = open(file, O_RDWR);
		vaddr = mmap(0, FULLSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	} else {
		vaddr = malloc(FULLSIZE);
	}

	purged = 0;
	vaddr += PAGE_SIZE-1;
	vaddr -= (long)vaddr % PAGE_SIZE;

	for(i=0; i < CHUNKNUM; i++)
		memset(vaddr + (i*CHUNK), 'A'+i, CHUNK);


	for(i=0; i < CHUNKNUM; ) {
		mvolatile(vaddr + (i*CHUNK), CHUNK);
		i+=2;
	}

//	for(i=0; i < CHUNKNUM; i++)
//		printf("%c\n", vaddr[i*CHUNK]);

	generate_pressure(pressure);

//	for(i=0; i < CHUNKNUM; i++)
//		printf("%c\n", vaddr[i*CHUNK]);

	/*for(i=0; i < CHUNKNUM; ) {
		int ret;
		ret = mnovolatile(vaddr + (i*CHUNK), CHUNK, &purged);
		i+=2;
	}*/

	if (purged)
		printf("Data purged!\n");
	for(i=0; i < CHUNKNUM; i++)
		printf("%c\n", vaddr[i*CHUNK]);
	


	return 0;
}

