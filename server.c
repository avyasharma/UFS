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
int fd, port, sd;
void * image;
int image_size;

// add function to lookup bitmap

int valid_inum(int inum) {
    // get pointer to bitmap
    unsigned int * inode_bitmap_ptr = s->inode_bitmap_addr * UFS_BLOCK_SIZE;
    // check if inode number is out of range
    int blocks = inum/4096;
    if(blocks > (*s).inode_bitmap_len) {
        return 0;
    }
    // get the valid bit
    int valid_byte = inode_bitmap_ptr[inum/32];
    int offset = 31 - (inum % 32);
    int valid_bit = (valid_byte >> offset) & 1;
    
    return valid_bit;
}

int find_free_inode() {
    unsigned int * inode_bitmap_ptr = (*s).inode_bitmap_addr * UFS_BLOCK_SIZE;
    int num_inodes = (*s).inode_bitmap_len*4096;
    int byte, offset, bit;
    for(int inum = 0; inum< num_inodes; inum++) {
        byte = inode_bitmap_ptr[inum/4];
        offset = 31 - (inum % 32);
        bit = (byte >> offset) & 1;
        if(bit != 1) {
            inode_bitmap_ptr[inum/4] = byte | (1 << offset);
            return inum;
        }
    }
    return -1;
}

int find_free_block() {
    unsigned int * data_bitmap_ptr = (*s).data_bitmap_addr * UFS_BLOCK_SIZE;
    int num_blocks = (*s).data_bitmap_len*4096;
    int byte, offset, bit;
    for(int idx = 0; idx< num_blocks; idx++) {
        byte = data_bitmap_ptr[idx/4];
        offset = 31 - (idx % 32);
        bit = (byte >> offset) & 1;
        if(bit != 1) {
            data_bitmap_ptr[idx/4] = byte | (1 << offset);
            return (*s).data_region_addr + idx*UFS_BLOCK_SIZE;
        }
    }
    return -1;
}

int Lookup(int pinum, char* name, struct sockaddr_in addr) {
    // check if pinum is valid
    if(valid_inum(pinum) == 0) {
        return -1;
    }

    // get directory
    inode_t inode = inode_table[pinum];
    if(inode.type != MFS_DIRECTORY) { // check if directory
        return -1;
    }
    // loop through each directory data block
    void* dir_ptr;
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t entry;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        if(inode.direct[i] == -1) {
            continue;
        }

        dir_block = (dir_block_t*)inode.direct[i];
        // loop through directory data block for each directory entry
        for(int i = 0; i <= num_dir_entries; i++) {
            entry = (dir_ent_t)(*dir_block).entries[i];
            // check if name match
            if (strcmp(entry.name, name)==0) { 
                if(entry.inum != -1) {
                    server_message_t msg;
                    msg.return_code = 0;
                    msg.stat.inode = entry.inum;
                    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));    
                    return entry.inum;
                }
            }
        }

    }
    server_message_t msg;
    msg.return_code = -1;
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    return -1;
}

int create_new(int pinum, int type){
    
    // find inum of a free inode
    int inum = find_free_inode();
    // get inode
    inode_t inode = inode_table[inum];
    inode.type = type;
    inode.size = sizeof(unsigned int);
    // find a free data block
    void* data_block = find_free_block();
    inode.direct[0] = data_block;
    // fill rest of inode with unused
    for(int i = 1; i< DIRECT_PTRS; i++) {
        inode.direct[i] = -1;
    }
    if(type == UFS_DIRECTORY) {
        // initialize directory data block
        dir_block_t* block = (dir_block_t *)data_block;
        strcpy((*block).entries[0].name, ".");
        (*block).entries[0].inum = inum;

        strcpy((*block).entries[1].name, "..");
        (*block).entries[1].inum = pinum;

        for(int i = 2; i < 128; i++)
        (*block).entries[i].inum = -1;    
    }
    // pwrite(fd, &inode, sizeof(inode_t), inode_table+inum*sizeof(inode_t));
    // pwrite(fd, &data_block, UFS_BLOCK_SIZE, data_block);
    // fsync(fd);
    msync(image, image_size, MS_SYNC);
    return inum;
}

void Create(int pinum, int type, char* name, struct sockaddr_in addr) {
    // check if name is too long
    if(strlen(name) > 28) {
        return -1;
    }
    // check if pinum is valid
    if(valid_inum(pinum)==-1) {
        return -1;
    }
    // check if file/directory already exists
    if(Lookup(pinum, name, addr) != -1) {
        return 0;
    }

    // get directory
    inode_t inode = inode_table[pinum];
    if(inode.type != MFS_DIRECTORY) { // check if directory
        return -1;
    }

    // loop through each directory data block and find empty spot
    void* dir_ptr;
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t entry;
    server_message_t* msg;
    msg -> return_code = -1;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        // if(inode.direct[i] == -1) {
        //     continue;
        // }

        dir_block = (dir_block_t*)inode.direct[i];
        // loop through directory data block for each directory entry
        for(int i = 0; i <= num_dir_entries; i++) {
            entry = (dir_ent_t)(*dir_block).entries[i];
            // if found empty directory entry
            if(entry.inum == -1) {
                strcpy(entry.name, name);
                entry.inum = create_new(pinum, type);
                msg->return_code = 0;
                msync(image, image_size, MS_SYNC);
                return 0;
            }
        }

    }
    UDP_Write(sd, &addr, (char*)msg, BUFFER_SIZE);
    //pwrite, fsync
    return -1;
}

void Write(int inum, char *buffer, int offset, int nbytes) {
    return 0;
}

void Stat(int inum, MFS_Stat_t *m, struct sockaddr_in addr) {
    if(valid_inum(inum) == -1) {
        return -1;
    }
    inode_t inode = inode_table[inum];
    MFS_Stat_t stat;
    stat.type = inode.type;
    stat.size = inode.size;
    
    char reply[BUFFER_SIZE];
    int rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
    if(rc == 0) {
        return 0;
    }
    return -1;
}

int Read(int inum, char *buffer, int offset, int nbytes) {
    if (!valid_inum(inum) || offset < 0 || nbytes < 0) {
        return -1;
    }

    // CHECK MATH AND EDGE CASES

    // read should fail if offset greater than file size or offset and nbytes is not a multiple of size of MFS_DirEnt_t

    // if inode not allocated, then return -1

    // if reading from directory

    // Get inode for given inode number
    inode_t inode = inode_table[inum];
    int i = offset / 4096;
    int num = nbytes / sizeof(MFS_DirEnt_t);
    MFS_DirEnt_t d[num];
    int total = offset % 4096 + nbytes;
    if (total < 4096) {
        if (inode.type == MFS_DIRECTORY) {
            for (int idx = 0; idx < num; i++) {
                int start = inode.direct[idx] * UFS_BLOCK_SIZE + (offset % 4096) + (idx * sizeof(MFS_DirEnt_t));
                pread(fd, &(d[i]), sizeof(MFS_DirEnt_t), start);
            }
        } else {
            pread(fd, buffer, nbytes, offset);  
        }
    } 
    if (nbytes - (offset % 4096)) {
        int start = 4096 - (offset % 4096);
        if (inode.type == MFS_DIRECTORY) {
            if (start / sizeof(MFS_DirEnt_t) <= num) {
                for (int x = 0; x < start / sizeof(MFS_DirEnt_t); x++) {
                    int begin = inode.direct[x] * UFS_BLOCK_SIZE + (offset % 4096) + (x * sizeof(MFS_DirEnt_t));
                    pread(fd, &(d[x]), sizeof(MFS_DirEnt_t), begin);
                }
                for (int j = 0; j < num - (start / sizeof(MFS_DirEnt_t)); j++) {
                    int begin = inode.direct[j] * UFS_BLOCK_SIZE + (offset % 4096) + (j * sizeof(MFS_DirEnt_t));
                    pread(fd, &(d[start / sizeof(MFS_DirEnt_t) + j]), sizeof(MFS_DirEnt_t), begin);
                }
            }

        } else {
            pread(fd, buffer, nbytes, inode.direct[i] * UFS_BLOCK_SIZE + (offset % 4096));  
            pread(fd, buffer, nbytes, inode.direct[i + 1] * UFS_BLOCK_SIZE);
        } 
    }
    
    return 0;
}

void Unlink(int pinum, char *name) {
    
}

void Shutdown(struct sockaddr_in addr) {
    server_message_t msg;
    msg.return_code = 0;
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    fsync(fd);
    msync(image, image_size, MS_SYNC);
    close(fd);
    exit(0);
}

// server code
int main(int argc, char *argv[]) {
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

    image_size = (int) sbuf.st_size;

    image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

    sd = UDP_Open(port);
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
            //printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
            switch (message.msg_type)
            {
            case MSG_LOOKUP:
                Lookup(message.lookup.pinum, message.lookup.name, addr);
                break;
            case MSG_CREATE:
                Create(message.create.pinum, message.create.type, message.create.name, addr);
                break;
            case MSG_WRITE:
                Write(message.write.inum, message.write.buffer, message.write.offset, message.write.nbytes);
                break;
            case MSG_STAT:
                Stat(message.stat.inum, message.stat.m, addr);
                break;
            case MSG_READ:
                Read(message.read.inum, message.read.buffer, message.read.offset, message.read.nbytes);
                break;
            case MSG_UNLINK:
                Unlink(message.unlink.pinum, message.unlink.name);
                break;
            case MSG_SHUTDOWN:
                Shutdown(addr);
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