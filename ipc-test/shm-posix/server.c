#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define SHARED "sharedfile"

int main() 
{
	int fd;
	int *data;

	fd = shm_open(SHARED, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

	if (fd < 0) {
		printf("error open region\n");
		return 0;
	}

	ftruncate(fd, 4);
	
	data = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		printf("error map\n");
		return 0;
	}

	*data = 100;

	printf("before sleep value %d\n", *data);
	while (*data != 200)
        	sleep(1);
	printf("check value after sleep %d\n", *data);

	exit(0);	
}
