#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>



void * mapCtl(){
	int pid;
	char *filename;
	pid = getpid();
	filename = (char *) calloc(265,1);
	sprintf(filename,"/tmp/%d.ctl",pid);
	printf("filename %s\n",filename);
	int fd = open(filename, O_RDONLY);
        if (fd < 0)
            perror("open");
	off_t len = lseek(fd, 0, SEEK_END);
        if (len == (off_t)-1)
            perror("lseek");
        void *data = mmap(0, len, PROT_READ , MAP_SHARED, fd, 0); 
        if (data == MAP_FAILED)
       		perror("mmap");
	return data;
}
u_int32_t force_key(void *data, u_int32_t *bitrate){
	if (data == MAP_FAILED) return 0;
	u_int8_t *cdata = (u_int8_t *) data;
	u_int32_t force_key = cdata[0];
	u_int32_t br = cdata[1];
	*bitrate = br * 100000;
	return force_key;
}

int32_t getIntEnv (char *name, int32_t defval){
	char *val = getenv(name);
	int32_t ival = defval;
	if (val != NULL){
		ival =atoi(val);
	}
	return ival;
}
int32_t getRotation(int32_t rot){
	return getIntEnv("AML_ROTATION",0);
}
int32_t getMirror(int32_t mir){
	return getIntEnv("AML_MIRROR",0);
}

