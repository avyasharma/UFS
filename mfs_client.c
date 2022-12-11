#include "udp.h"
#include "ufs.h"
#include "mfs.h"
#include "message.h"

struct sockaddr_in server_addr;
int fd;

int MFS_Init(char *hostname, int port) {
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    if(rc != 0) {
        printf("Failed to set up server address\n");
        return rc;
    }
    fd = UDP_Open(12121);
    return 0;
}

int MFS_Lookup(int pinum, char *name) {

    return 0;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    client_message_t message;
    server_message_t response;

    message.msg_type = 2;
    message.stat.inum = inum;
    message.m = m;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    


    return 0;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    return 0;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes) {
    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    return 0;
}

int MFS_Unlink(int pinum, char *name) {
    return 0;
}

int MFS_Shutdown() {
    return 0;
}