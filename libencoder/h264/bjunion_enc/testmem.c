#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>



int main(int ac, char *av[])
{

	int pid;
	char *filename;
	pid = getpid();
	filename = calloc(265,1);
	sprintf(filename,"/tmp/%d.ctl",pid);
	printf("filename %s\n",filename);
	int fd = open(filename, O_RDONLY);
        if (fd < 0)
            perror("open");
	off_t len = lseek(fd, 0, SEEK_END);
        if (len == (off_t)-1)
            perror("lseek");
        unsigned char* data = mmap(0, len, PROT_READ , MAP_SHARED, fd, 0); 
        if (data == MAP_FAILED)
       		perror("mmap");
	int force_key = data[0];
	int bitrate = data[1];
	printf("values %d %d \n",force_key,bitrate);

}

