#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define SHMSZ    4
#define PREFIX 123 

main()
{
    char c;
    int shmid;
    key_t key;
    char *shm;
    int  *s;

    /*
     * We'll name our shared memory segment
     * "5678".
     */
    key = PREFIX * 10 + 1;

    /*
     * Create the segment.
     */
    if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }

    /*
     * Now put some things into the memory for the
     * other process to read.
     */
    s = (int *)shm;

   /* for (c = 'a'; c <= 'z'; c++)
        *s++ = c;
f ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }    *s = NULL;*/
    *s = 100;
    printf("before sleep value %d\n", *shm);

    /*
     * Finally, we wait until the other process 
     * changes the first character of our memory
     * to '*', indicating that it has read what 
     * we put there.
     */
    while (*s != 200)
        sleep(1);

   printf("check value after sleep %d\n", *s);

    exit(0);
}
