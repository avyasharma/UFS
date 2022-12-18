#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>

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
int debug = 0;

// add function to lookup bitmap

int valid_inum(int inum) {
    // get pointer to bitmap
    unsigned int * inode_bitmap_ptr = image + s->inode_bitmap_addr * UFS_BLOCK_SIZE;
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

unsigned int find_free_inode() {
    unsigned int * inode_bitmap_ptr = image + (*s).inode_bitmap_addr * UFS_BLOCK_SIZE;
    int num_inodes = (*s).inode_bitmap_len*4096;
    int byte, offset, bit;
    for(int inum = 0; inum < num_inodes; inum++) {
        byte = inode_bitmap_ptr[inum/32];
        offset = 31 - (inum % 32);
        bit = (byte >> offset) & 1;
        if(bit != 1) {
            inode_bitmap_ptr[inum/32] = byte | (1 << offset);
            return inum;
        }
    }
    return -1;
}

unsigned int get_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |= 0x1 << offset;
}

unsigned int find_free_block() {
    unsigned int * data_bitmap_ptr = image + (*s).data_bitmap_addr * UFS_BLOCK_SIZE;
    int num_blocks = (*s).data_bitmap_len*4096;
    int byte, offset, bit;
    for(int idx = 0; idx< num_blocks; idx++) {
        // byte = data_bitmap_ptr[idx/32];
        // offset = 31 - (idx % 32);
        // bit = (byte >> offset) & 1;
        bit = get_bit(data_bitmap_ptr, idx);
        if(bit != 1) {
            data_bitmap_ptr[idx/32] = byte | (1 << offset);
            set_bit(data_bitmap_ptr, idx);
            return idx+(*s).data_region_addr;
        }
    }
    return -1;
}

int Lookup(int pinum, char* name, struct sockaddr_in addr) {
    server_message_t msg;
    msg.return_code = -1;
    printf("LOOKUP STARTS HERE...finding %s\n", name);
    // check if pinum is valid
    if(valid_inum(pinum) == 0) {
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }

    // get directory
    inode_t inode = inode_table[pinum];
    if(inode.type != MFS_DIRECTORY) { // check if directory
        UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
        return -1;
    }
    // loop through each directory data block
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t * entry;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        // printf("Checking for allocation\n");
        if(inode.direct[i] == -1) {
            continue;
        }
        // dir_ptr = (void*)inode.direct[i];
        // dir_block = (dir_block_t*)dir_ptr;
        if (debug == 3) printf("Getting the data block\n");
        // memcpy(&dir_block, &inode.direct[i], sizeof(unsigned int));
        dir_block = (dir_block_t*)(image + inode.direct[i] * UFS_BLOCK_SIZE);
        // dir_block = (dir_block_t*) *inode.direct[i];
        // loop through directory data block for each directory entry
        for(int i = 0; i < num_dir_entries; i++) {
            //printf("Currently looping through...\n");
            // entry = (dir_ent_t)(*dir_block).entries[i];
            //printf("dir_block %p\n", dir_block);
            entry = (dir_ent_t*)&(dir_block->entries[i]);
            printf("DIR ENTRY: %d, %s\n", entry->inum, entry->name);
            //printf("Got the current entry...\n");
            // check if name match
            if (strcmp(entry->name, name)==0) {
                printf("Match found..\n"); 
                if(entry->inum != -1) {
                    printf("Return code writing\n");
                    
                    msg.return_code = 0;
                    msg.stat.inode = entry->inum;
                    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));    
                    return entry->inum;
                }
            }
        }
    }

    if (debug == 3) printf("Sending stuff back :)\n");
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    return -1;
}

int Lookup_helper(int pinum, char* name) {
    if(valid_inum(pinum) == 0) {
        return -1;
    }

    // get directory
    inode_t inode = inode_table[pinum];
    // if(inode.type != MFS_DIRECTORY) { // check if directory
    //     return -1;
    // }
    // loop through each directory data block
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t * entry;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        // printf("Checking for allocation\n");
        if(inode.direct[i] == -1) {
            continue;
        }
        // dir_ptr = (void*)inode.direct[i];
        // dir_block = (dir_block_t*)dir_ptr;
        printf("Getting the data block\n");
        // memcpy(&dir_block, &inode.direct[i], sizeof(unsigned int));
        dir_block = (dir_block_t*)(image + inode.direct[i] * UFS_BLOCK_SIZE);
        // dir_block = (dir_block_t*) *inode.direct[i];
        // loop through directory data block for each directory entry
        for(int i = 0; i < num_dir_entries; i++) {
            //printf("Currently looping through...\n");
            // entry = (dir_ent_t)(*dir_block).entries[i];
            //printf("dir_block %p\n", dir_block);
            entry = (dir_ent_t*)&(dir_block->entries[i]);
            //printf("DIR ENTRY: %d, %s\n", entry->inum, entry->name);
            //printf("Got the current entry...\n");
            // check if name match
            
            if (strcmp(entry->name, name)==0) {
                printf("Match found..\n"); 
                if(entry->inum != -1) {  
                    return entry->inum;
                }
            }
        }

    }
    return -1;
}



int create_new(int pinum, int type){
    // TO DO: Increment Size


    // find inum of a free inode

    printf("INDING FREE INODE\n");
    int inum = find_free_inode();
    printf("FOUND FREE INODE\n");
    // get inode
    inode_t* inode = &inode_table[inum];
    inode->type = type;
    inode->size = sizeof(unsigned int);
    // find a free data block
    unsigned int data_block = find_free_block();
    printf("FOUND FREE BLOCK\n");
    inode->direct[0] = data_block;
    // fill rest of inode with unused
    for(int i = 1; i< DIRECT_PTRS; i++) {
        inode->direct[i] = -1;
    }
    if(type == MFS_DIRECTORY) {
        // initialize directory data block
        // dir_block_t* block = (dir_block_t *)data_block;
        dir_block_t* block = image + data_block*UFS_BLOCK_SIZE;

        if (debug == 2) printf("Getting directory\n");

        //memcpy(block, &data_block, sizeof(unsigned int));

        if (debug == 2) printf("Copied data block\n");
        strcpy((*block).entries[0].name, ".");

        if (debug == 2) printf("Got `.` path in\n");
        (*block).entries[0].inum = inum;

        if (debug == 2) printf("Got `..` path in\n");
        strcpy((*block).entries[1].name, "..");

        (*block).entries[1].inum = pinum;

        if (debug == 2) printf("Setting up directory\n");
        for(int i = 2; i < 128; i++)
        (*block).entries[i].inum = -1;  
        // memcpy(data_block, block)  
    }
    // pwrite(fd, &inode, sizeof(inode_t), inode_table+inum*sizeof(inode_t));
    // pwrite(fd, &data_block, UFS_BLOCK_SIZE, data_block);
    // fsync(fd);
    msync(image, image_size, MS_SYNC);
    return inum;
}

int Create(int pinum, int type, char* name, struct sockaddr_in addr) {
    server_message_t msg;
    msg.return_code = -1;
    printf("STARTING CREATE\n");
    // check if name is too long
    if(strlen(name) > 28) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
        return -1;
    }
    // check if pinum is valid

    printf("BEFORE LOOKUP HELPER\n");
    if(valid_inum(pinum)==-1) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
        return -1;
    }
    // check if file/directory already exists
    if(Lookup_helper(pinum, name) != -1) {
        msg.return_code = 0;
        UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
        return 0;
    }
    printf("AFTER LOOKUP HELPER\n");
    if (debug) printf("PASSED CHECKS\n");
    // get directory
    inode_t inode = inode_table[pinum];
    printf("INODE TYPE: %d, SIZE: %d\n, INUM: %d", inode.type, inode.size, pinum);
    if(inode.type != MFS_DIRECTORY) { // check if directory
        msg.return_code = -1;
        UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
        return -1;
    }
    printf("GOT DIRECTORY\n");
    // loop through each directory data block and find empty spot
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t * entry;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        if(inode.direct[i] == -1) {
            continue;
        }

        // dir_block = (dir_block_t*)inode.direct[i];
        // memcpy(dir_block, &inode.direct[i], sizeof(unsigned int));
        if (debug == 1) printf("HERE\n");
        dir_block = image + inode.direct[i] * UFS_BLOCK_SIZE;
        // loop through directory data block for each directory entry
        for(int i = 0; i < num_dir_entries; i++) {
            entry = (dir_ent_t*)&(*dir_block).entries[i];
            // if found empty directory entry
            if(entry->inum == 2) {
                printf("INUM 2 FOUND\n");
            }
            if(entry->inum == -1) {
                strcpy(entry->name, name);
                entry->inum = create_new(pinum, type);
                printf("INUM CREATED :%d\n", entry->inum);
                msg.return_code = 0;
                msync(image, image_size, MS_SYNC);
                break;
            }
        }

    }
    if (debug == 1) printf("RETURNING CODE: %d\n", msg.return_code);
    UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
    //pwrite, fsync
    return -1;
}

int Write(int inum, char *buffer, int offset, int nbytes, struct sockaddr_in addr) {
    printf("WRITE STARTED\n");
    printf("NBYTES FROM WRITE %d\n", nbytes);
    printf("OFFSET FROM WRITE %d\n", offset);
    if (debug == 1) printf("In write\n");
    server_message_t msg;
    // check if valid inum
    if(valid_inum(inum)==-1) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }
    
    if (debug == 1) printf("PASSED CHECKER 1\n");
    // get directory
    if (debug == 1) printf("inum: %d\n", inum);
    inode_t inode = inode_table[inum];
    // check if regular file
    if (debug == 1) printf("Inode Type: %d\n", inode.type);
    if (debug == 1) printf("Printed type\n");
    if(inode.type != UFS_REGULAR_FILE) { // check if directory
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }

    if (debug == 1) printf("PASSED CHECKER 2\n");
    // check invalid nbytes
    if(nbytes > 4096 || nbytes<0) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }
    // check invalid offset
    int start_disk = offset/4096;
    int block_off = offset % 4096;
    // int start_offset = offset % 4096;
    // int end_disk = (offset+nbytes)/4096;
    // int end_offset = (offset+nbytes)%4096;
    // if(start_disk < 0 || end_disk >= DIRECT_PTRS) {
    //     return -1;
    // }
    if (debug == 1) printf("PASSED CHECKER 3\n");
    // pread(fd, &(d[i]), sizeof(MFS_DirEnt_t), start);
    // void *wptr = image + start_disk * UFS_BLOCK_SIZE + start;

    // check if data block has been allocated
    if(inode.direct[start_disk] == -1) {
        inode.direct[start_disk] = find_free_block();
    }
    // printf("start_disk: %d\n", start_disk);
    // printf("start_offset: %d\n", start_offset);
    // void *wptr = image + inode.direct[start_disk]*UFS_BLOCK_SIZE + start_offset;
    // printf("POINTER: %p\n", wptr);
    // if (end_disk == start_disk) {
    //     printf("ENTERED IF \n");
    //     printf("BUFFER %s\n", buffer);
    //     printf("NBYTES: %d\n", nbytes);
    //     memcpy(wptr, buffer, nbytes);
    //     msync(image, image_size, MS_SYNC);
    // } else {
    //     int first = UFS_BLOCK_SIZE - start_offset;
    //     memcpy(wptr, buffer, first);
    //     void *eptr = image + inode.direct[end_disk] * UFS_BLOCK_SIZE;
    //     memcpy(eptr, buffer, nbytes - first);
    //     msync(image, image_size, MS_SYNC);
    // }
    // int start = offset / 4096;
    printf("Inode.direct[start_Disk] in write: %d\n", inode.direct[start_disk]);
    printf("Offset in write: %x\n", inode.direct[start_disk] * UFS_BLOCK_SIZE + block_off);
    pwrite(fd, buffer, nbytes, inode.direct[start_disk] * UFS_BLOCK_SIZE + block_off);
    

    msg.return_code = 0;
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    fsync(fd);
    return 0;
}

int Stat(int inum, MFS_Stat_t *m, struct sockaddr_in addr) {
    server_message_t msg;
    
    // printf("Here 1\n");
    if(valid_inum(inum) == -1) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    } else {
        printf("Type %d\n", inode_table[inum].type);

        MFS_Stat_t stat;
        m = &stat;
        m->type = inode_table[inum].type;
        m->size = inode_table[inum].size;
        m->inode = inum;
    }

    // printf("Here 2\n");
    // inode_t inode = inode_table[inum];

    // MFS_Stat_t stat;

    // printf("Here 3\n");
    // // inode_t inode = inode_table[inum];
    // printf("Inode type: %d\n", inode.type);
    // stat.type = inode.type;

    // printf("Here 4\n");
    // stat.size = inode.size;

    // printf("Here 5\n");
    // stat.inode = inum;    

    // printf("Here 6\n");
    msg.return_code = 0;
    msg.stat.type = m->type;
    msg.stat.size = m->size;
    msg.stat.inode = inum;
    // memcpy(&msg.stat, &stat, sizeof(MFS_Stat_t));
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    
    // char reply[BUFFER_SIZE];
    // int rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
    // if(rc == 0) {
    //     return 0;
    // }

    return 0;
}

int Read(int inum, char *buffer, int offset, int nbytes, struct sockaddr_in addr) {
    printf("START READ\n");
    server_message_t msg;
    if (!valid_inum(inum) || offset < 0 || nbytes < 0 || nbytes > 4096) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }


    inode_t inode = inode_table[inum];

    int start_disk = offset/4096;
    int block_off = offset % 4096;
    // int start_offset = offset % 4096;
    // int end_disk = (offset+nbytes)/4096;
    // int end_offset = (offset+nbytes)%4096;
    // if(start_disk < 0 || end_disk >= DIRECT_PTRS) {
    //     return -1;
    // }

    // void *wptr = image + start_disk * UFS_BLOCK_SIZE + start;
    // void *wptr = image + inode.direct[start_disk]*UFS_BLOCK_SIZE + start_offset;
    // if (end_disk == start_disk) {
    //     memcpy(buffer, wptr, nbytes);
    // } else {
    //     int first = UFS_BLOCK_SIZE - start_offset;
    //     memcpy(buffer, wptr, first);
    //     void *eptr = image + end_disk * UFS_BLOCK_SIZE;
    //     void *bufoff = buffer + first;
    //     memcpy(bufoff, eptr, nbytes - first);
    // }

    if(inode.direct[start_disk] == -1) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }


    printf("INODE.DIRECT[START_DISK]: %d\n", inode.direct[start_disk]);

    pread(fd, msg.buffer, nbytes, inode.direct[start_disk] * UFS_BLOCK_SIZE + block_off);
    

    msg.return_code = 0;
    // memcpy(msg.buffer, &buffer, nbytes);
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    fsync(fd);
    return 0;

    // CHECK MATH AND EDGE CASES

    // read should fail if offset greater than file size or offset and nbytes is not a multiple of size of MFS_DirEnt_t

    // if inode not allocated, then return -1

    // if reading from directory

    // Get inode for given inode number
    // inode_t inode = inode_table[inum];
    // int i = offset / 4096;
    // int num = nbytes / sizeof(MFS_DirEnt_t);
    // MFS_DirEnt_t d[num];
    // int total = offset % 4096 + nbytes;
    // if (total < 4096) {
    //     if (inode.type == MFS_DIRECTORY) {
    //         for (int idx = 0; idx < num; i++) {
    //             int start = inode.direct[idx] * UFS_BLOCK_SIZE + (offset % 4096) + (idx * sizeof(MFS_DirEnt_t));
    //             pread(fd, &(d[i]), sizeof(MFS_DirEnt_t), start);
    //         }
    //     } else {
    //         pread(fd, buffer, nbytes, offset);  
    //     }
    // } 
    // if (nbytes - (offset % 4096)) {
    //     int start = 4096 - (offset % 4096);
    //     if (inode.type == MFS_DIRECTORY) {
    //         if (start / sizeof(MFS_DirEnt_t) <= num) {
    //             for (int x = 0; x < start / sizeof(MFS_DirEnt_t); x++) {
    //                 int begin = inode.direct[x] * UFS_BLOCK_SIZE + (offset % 4096) + (x * sizeof(MFS_DirEnt_t));
    //                 pread(fd, &(d[x]), sizeof(MFS_DirEnt_t), begin);
    //             }
    //             for (int j = 0; j < num - (start / sizeof(MFS_DirEnt_t)); j++) {
    //                 int begin = inode.direct[j] * UFS_BLOCK_SIZE + (offset % 4096) + (j * sizeof(MFS_DirEnt_t));
    //                 pread(fd, &(d[start / sizeof(MFS_DirEnt_t) + j]), sizeof(MFS_DirEnt_t), begin);
    //             }
    //         }

    //     } else {
    //         pread(fd, buffer, nbytes, inode.direct[i] * UFS_BLOCK_SIZE + (offset % 4096));  
    //         pread(fd, buffer, nbytes, inode.direct[i + 1] * UFS_BLOCK_SIZE);
    //     } 
    // }
}

int directory_empty(int inum) {
    printf("CHECKING DIRECTORY WIHT INUM: %d\n", inum);
    inode_t inode = inode_table[inum];
    if(inode.type != MFS_DIRECTORY) { // check if directory
        return 0;
    }
    printf("Passed first check\n");
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t * entry;
    int allocated = 0;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        if(inode.direct[i] == -1) {
            continue;
        }
        dir_block = image + inode.direct[i] * UFS_BLOCK_SIZE;
        // loop through directory data block for each directory entry
        for(int i = 0; i < num_dir_entries; i++) {
            entry = (dir_ent_t*)&(dir_block->entries[i]);
            // if found match
            if(entry->inum != -1){
                printf("INUM FOUND %d, %sEND\n", entry->inum, entry->name);
                allocated++;
            }
        }

    }
    printf("ALLOCated: %d\n", allocated);
    if(allocated > 2) {
        return -1;
    }
    return 0;
}

int Unlink(int pinum, char *name, struct sockaddr_in addr) {
    printf("START UNLINK\n");
    server_message_t msg;
    // check if valid inum
    if(valid_inum(pinum)==-1) {
        msg.return_code = -1;
        UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
        return -1;
    }

    inode_t inode = inode_table[pinum];
    if(inode.type != MFS_DIRECTORY) { // check if directory
        msg.return_code = -1;
        UDP_Write(sd, &addr, (char*)&msg, BUFFER_SIZE);
        return -1;
    }
    // printf("GOT DIRECTORY\n");
    // loop through each directory data block and find empty spot
    dir_block_t* dir_block;
    int num_dir_entries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
    dir_ent_t * entry;
    for(int i = 0; i< DIRECT_PTRS; i++) {
        if(inode.direct[i] == -1) {
            continue;
        }

        // dir_block = (dir_block_t*)inode.direct[i];
        // memcpy(dir_block, &inode.direct[i], sizeof(unsigned int));
        if (debug == 1) printf("HERE\n");
        dir_block = image + inode.direct[i] * UFS_BLOCK_SIZE;
        // loop through directory data block for each directory entry
        for(int i = 0; i < num_dir_entries; i++) {
            entry = (dir_ent_t*)&(dir_block->entries[i]);
            // if found match
            if(strcmp(entry->name, name)==0) {
                if(directory_empty(entry->inum) == 0){
                    printf("SUCCESS\n");
                    // return success
                    entry->inum = -1;
                    msync(image, image_size, MS_SYNC);
                    msg.return_code = 0;
                    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
                    return 0;
                } else{
                    // return failure
                    msg.return_code = -1;
                    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
                    return -1;
                }
            }
        }

    }

    // check if directory is empty
    msg.return_code = 0;
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    return 0;
}

void Shutdown(struct sockaddr_in addr) {
    server_message_t msg;
    msg.return_code = 0;
    UDP_Write(sd, &addr, (void *)&msg, sizeof(msg));
    fsync(fd);
    //msync(image, image_size, MS_SYNC);
    close(fd);
    exit(0);
}

void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

// server code
int main(int argc, char *argv[]) {
    printf("SERVER STARTED\n");
    signal(SIGINT, intHandler);

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

    sd = UDP_Open(port);
    assert(sd > -1);

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

    // loop through requests
    while (1) {
        struct sockaddr_in addr;
        // char message[BUFFER_SIZE];
        client_message_t msg;
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char*)&msg, sizeof(msg));
        // printf("Message from client: %d\n", msg.msg_type);
        if (rc > 0) {
            // add some code to interpret message
            client_message_t message = msg;
            //printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
            switch (message.msg_type)
            {
            case MSG_LOOKUP:
                //printf("Lookin up from server\n");
                printf("FROM SWITCH STATEMENT: %s\n", msg.lookup.name);
                Lookup(message.lookup.pinum, msg.lookup.name, addr);
                break;
            case MSG_CREATE:
                if (debug == 1) printf("CREATE from server\n");
                Create(message.create.pinum, message.create.type, message.create.name, addr);
                break;
            case MSG_WRITE:
                printf("ABOUT TO CALL WRITE. NBYTES: %d OFFSET: %d\n", message.write.nbytes, message.write.offset);
                Write(message.write.inum, message.write.buffer, message.write.offset, message.write.nbytes, addr);
                break;
            case MSG_STAT:
                Stat(message.stat.inum, message.stat.m, addr);
                break;
            case MSG_READ:
                Read(message.read.inum, message.read.buffer, message.read.offset, message.read.nbytes, addr);
                break;
            case MSG_UNLINK:
                Unlink(message.unlink.pinum, message.unlink.name, addr);
                break;
            case MSG_SHUTDOWN:
                printf("SHUTDOWN from server\n");
                Shutdown(addr);
                break;
            }

            // char reply[BUFFER_SIZE];
            // sprintf(reply, "goodbye world");
            // rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            // printf("server:: reply\n");
        } 
    }
    return 0; 
}