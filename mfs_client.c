#include "udp.h"
#include "ufs.h"
#include "mfs.h"
#include "message.h"
#include "server.c"
#include <time.h>
#include <stdlib.h>

struct sockaddr_in server_addr;
int fd;
int connected = 0;

int MIN_PORT = 20000;
int MAX_PORT = 40000;

int MFS_Init(char *hostname, int port) {
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    if(rc != 0) {
        printf("Failed to set up server address\n");
        return rc;
    }
    srand(time(0));
    int port_num = (rand() % (MAX_PORT - MIN_PORT) + MIN_PORT);
    fd = UDP_Open(port_num);
    connected = 1;
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    client_message_t message;
    server_message_t response;
    
    if (!connected) {
        return -1;
    } 
    if (pinum < 0) {
        return -1;
    } 
    if (strlen(name) > 28 || name == NULL) {
        return -1;
    }

    message.msg_type = 1;
    message.lookup.pinum =  pinum;
    strncpy(message.lookup.name, name, strlen(name));
    int rc = UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));
    
    struct sockaddr_in ret_addy;
    int rc_ret = UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    return response.stat.inode;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    client_message_t message;
    server_message_t response;

    if (inum < 0) {
        return -1;
    }

    message.msg_type = 4;
    message.stat.inum = inum;
    message.stat.m = m;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message)); // sending message to server

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response)); // get response from server.
    m->type = response.stat.type;
    m->size = response.stat.size;

    return 0;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    client_message_t message;
    server_message_t response;

    if (inum < 0) {
        return -1;
    }
    if (nbytes > 4096) {
        return -1;
    }
    if (offset < 0) {
        return -1;
    }

    message.msg_type = 3;
    message.stat.inum = inum;
    memcpy(message.write.buffer, buffer, nbytes);
    message.write.offset = offset;
    message.write.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    if (!response.stat.type) {
        return -1;
    }
    return 0;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes) {
    client_message_t message;
    server_message_t response;

    if (inum < 0) {
        return -1;
    }
    if (nbytes > 4096) {
        return -1;
    }
    if (offset < 0) {
        return -1;
    }

    message.msg_type = 5;
    message.stat.inum = inum;
    memcpy(message.read.buffer, buffer, nbytes);
    message.read.offset = offset;
    message.read.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    if (!response.stat.type) {
        // figure this out
        return 0;
    }

    memcpy(buffer, response.buffer, nbytes);
    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    client_message_t message;
    server_message_t response;

    if (pinum < 0) {
        return -1;
    }
    if (strlen(name) > 28 || name == NULL) {
        return -1;
    }

    message.msg_type = 2;
    message.create.pinum = pinum;
    message.create.type = type;
    strncpy(message.create.name, name, strlen(name));

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    
    return response.return_code;
}

int MFS_Unlink(int pinum, char *name) {
    client_message_t message;
    server_message_t response;

    if (pinum < 0) {
        return -1;
    }
    if (strlen(name) > 28 || name == NULL) {
        return -1;
    }

    message.msg_type = 2;
    message.unlink.pinum = pinum;
    strncpy(message.unlink.name, name, strlen(name));

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    
    return response.return_code;
}

int MFS_Shutdown() {
    client_message_t message;
    server_message_t response;

    message.msg_type = 7;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    //exit(0);
    //UDP_Close(fd);
    return response.return_code;
}