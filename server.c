#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "udp.h"
#include "ufs.h"
#include "message.h"

#define BUFFER_SIZE (1000)

super_t* s;
inode_t* inode_table;
inode_t* root_inode;
dir_ent_t* root_dir;

// add function to lookup bitmap

int get_block(pinum) {
    int num_inode_entries = (*inode_table).size/sizeof(unsigned int);
    if (pinum>=num_inode_entries) {
        return -1;
    }
    return (*inode_table).direct[pinum];
}

int lookup(int pinum, char* name) {
    // get directory
    void* directory = get_block(pinum);
    if(directory == -1) {
        return -1;
    }
    // get number of entries in directory
    int num_inode_entries = (*(inode_t*)directory).size/sizeof(unsigned int);
    
    // search through directory for file/directory with specified name
    dir_ent_t* entry;
    for(int i = 0; i <= num_inode_entries ; i += 1) {
        entry = (dir_ent_t*)(directory+i * sizeof(dir_ent_t));
        if (strcmp((*entry).name, name)==0) {
            return (*entry).inum;
        }
    }
    return -1;
}

void create(int pinum, int type, char* name) {
    // check if name is too long
    if(strlen(name) > 28) {
        return -1;
    }
    // get directory with pinum
    void* directory = get_block(pinum);
    if(directory == -1) {
        return -1;
    }
    // get number of entries in directory
    int num_inode_entries = (*(inode_t*)directory).size/sizeof(unsigned int);
    
    // check if name exists
    dir_ent_t* entry;
    for(int i = 0; i <= num_inode_entries ; i += 1) {
        entry = (dir_ent_t*)(directory+i * sizeof(dir_ent_t));
        if (strcmp((*entry).name, name)==0) {
            return 0;
        }
    }

    // search for unused inode


    // create file/directory
    if (type == MFS_REGULAR_FILE) {
        
        //directory + num_inode_entries * sizeof(dire_ent_t) = ;
    } else {
        
    }

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
    assert(fd > -1);

    struct stat sbuf;
    int rc = fstat(fd, &sbuf);
    assert(rc > -1);

    int image_size = (int) sbuf.st_size;

    void *image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(image != MAP_FAILED);

    // get pointers to file system
    s = (super_t *) image;
    // printf("inode bitmap address %d [len %d]\n", s->inode_bitmap_addr, s->inode_bitmap_len);
    // printf("data bitmap address %d [len %d]\n", s->data_bitmap_addr, s->data_bitmap_len);

    inode_table = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    root_inode = inode_table;
    // printf("\nroot type:%d root size:%d\n", root_inode->type, root_inode->size);
    // printf("direct pointers[0]:%d [1]:%d\n", root_inode->direct[0], root_inode->direct[1]);

    root_dir = image + (root_inode->direct[0] * UFS_BLOCK_SIZE);
    // printf("\nroot dir entries\n%d %s\n", root_dir[0].inum, root_dir[0].name);
    // printf("%d %s\n", root_dir[1].inum, root_dir[1].name);

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
                lookup(message.lookup.pinum, message.lookup.name);
                break;
            case MSG_CREATE:
                create(message.create.pinum, message.create.type, message.create.name);
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