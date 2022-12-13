#include <stdio.h>
#include "udp.h"
#include "message.h"

#define BUFFER_SIZE (1000)

void lookup(int pinum, char* name) {

}

void create(int pinum, int type, char* name) {

}

void write(int inum, char *buffer, int offset, int nbytes) {

}

void stat(int inum, MFS_Stat_t *m) {

}

void read(int inum, char *buffer, int offset, int nbytes) {
    
}

void unlink(int pinum, char *name) {
    
}


// server code
int main(int argc, char *argv[]) {
    int fd, port;
    // get input
    if(argc > 1) {
        port = atoi(argv[1]);
    }
    if(argc > 2) {
        fd = open(argv[2], O_RDWR);
    } else{
        printf("image does not exist\n");
    }

    int sd = UDP_Open(port);
    assert(sd > -1);

    // loop through requests
    while (1) {
        struct sockaddr_in addr;
        // char message[BUFFER_SIZE];
        client_message_t* msgptr;
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char*)msgptr, BUFFER_SIZE);
        if (rc > 0) {
            // add some code to interpret message
            client_message_t message = *msgptr;
            printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
            switch (message.msg_type)
            {
            case MSG_LOOKUP:
                lookup(message.create.pinum, message.create.name);
                break;
            case MSG_CREATE:
                create(message.create.pinum, message.create.type, message.name);
                break;
            case MSG_WRITE:
                write(message.write.inum, message.write.buffer, message.write.offset, message.write.nbytes);
                break;
            case MSG_STAT:
                stat(message.stat.inum, message.stat.m);
                break;
            case MSG_READ:
                read(message.read.inum, message.read.buffer, message.read.offset, message.read.nbytes);
                break;
            case MSG_UNLINK:
                unlink(message.unlink.pinum, message.unlink.name);
                break;
            }

            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        } 
    }
    return 0; 
}