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
    // fd = UDP_Open(port);
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

    message.msg_type = MSG_LOOKUP;
    message.lookup.pinum =  pinum;
    strcpy(message.lookup.name, name);
    printf("SENDING MESSAGE TO LOOKUP FILE... looking for %s\n", message.lookup.name);
    int rc = UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));
    
    struct sockaddr_in ret_addy;
    int rc_ret = UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    printf("Return code: %d\n", response.return_code);
    if(response.return_code == -1){
        return -1;
    }
    printf("Inode found: %d\n", response.stat.inode);
    return response.stat.inode;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    client_message_t message;
    server_message_t response;

    if (inum < 0) {
        return -1;
    }

    message.msg_type = MSG_STAT;
    message.stat.inum = inum;
    message.stat.m = m;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message)); // sending message to server

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response)); // get response from server.
    // printf("Before mem copy\n");
    printf("Response type: %d\n", response.stat.type);
    printf("Response size: %d\n", response.stat.size);
    printf("Response inum: %d\n", response.stat.inode);
    // printf("M type initial: %d\n", m->type);
    m->type = response.stat.type;
    // printf("Mem type: %d\n", m->type);
    // printf("Before mem copy 1\n");
    m->size = response.stat.size;
    // printf("Before mem copy 1\n");
    m->inode = response.stat.inode;
    // printf("After mem copy\n");
    // memcpy(m, &response.stat, sizeof(MFS_Stat_t));
    
    printf("m type: %d\n", m->type);

    return response.return_code;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    client_message_t message;
    server_message_t response;

    // if (inum < 0) {
    //     return -1;
    // }
    // if (nbytes > 4096) {
    //     return -1;
    // }
    // if (offset < 0) {
    //     return -1;
    // }

    // printf("TYPE ASSIGNED\n");
    message.msg_type = MSG_WRITE;
    message.write.inum = inum;
    // printf("BEFORE MEMCPY\n");
    memcpy(message.write.buffer, buffer, nbytes);
    message.write.offset = offset;
    message.write.nbytes = nbytes;
    printf("ABOUT TO CALL WRITE. BUFFER: %s\n", message.write.buffer);
    printf("NBYTES IN MFS CLIENT :%d\n", message.write.nbytes);
    printf("OFFSET IN MFS CLIENT :%d\n", message.write.offset);
    // printf("BEFORE ");
    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    printf("Write return code: %d\n", response.return_code);
    if (response.return_code == -1) {
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

    message.msg_type = MSG_READ;
    message.stat.inum = inum;
    // memcpy(message.read.buffer, &buffer, nbytes);
    message.read.offset = offset;
    message.read.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    printf("READ Return code: %d\n", response.return_code);
    if (response.return_code == -1) {
        return -1;
    }
    printf("BUFFER in MFS_READ: %s\n", response.buffer);
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

    message.msg_type = MSG_CREATE;
    message.create.pinum = pinum;
    message.create.type = type;
    strcpy(message.create.name, name);
    printf("SENDING MESSAGE TO CREATE FILE\n");
    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));
    printf("DONE WRITING\n");
    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    printf("FINISHED\n");
    printf("Return code: %d\n", response.return_code);
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

    message.msg_type = MSG_UNLINK;
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

    message.msg_type = MSG_SHUTDOWN;

    UDP_Write(fd, &server_addr, (char*)&message, sizeof(message));

    struct sockaddr_in ret_addy;

    UDP_Read(fd, &ret_addy, (char*)&response, sizeof(response));
    //exit(0);
    //UDP_Close(fd);
    return response.return_code;
}