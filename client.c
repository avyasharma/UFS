#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    char *hostname = argv[1];
    int port = atoi(argv[2]);
    char *cmd = argv[3];

    int rc = MFS_Init(hostname, port);

    if (rc !=0 ) {
        return -1;
    }

    if (strcmp(cmd, "stat") == 0) {
        int inum = atoi(argv[4]);

        MFS_Stat_t stat;
        int ret = MFS_Stat(inum, &stat);

        if (ret != 0) {
            printf("error\n");
        } else {
            printf("file type %i\n", stat.type);
        }

        return 0;
    } else{
        printf("Got unexpected command: %s\n", cmd);
    }

    // struct sockaddr_in addrSnd, addrRcv;

    // int sd = UDP_Open(20000);
    // int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    // char message[BUFFER_SIZE];
    // sprintf(message, "hello world");

    // printf("client:: send message [%s]\n", message);
    // rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    // if (rc < 0) {
	// printf("client:: failed to send\n");
	// exit(1);
    // }

    // printf("client:: wait for reply...\n");
    // rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    // printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
    // return 0;
}