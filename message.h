#ifndef MESSAGE_H
#define MESSAGE_H

#define MAX_NAME_LEN 28
#define MAX_BUFFER_SIZE 4096

#define MSG_LOOKUP 1
#define MSG_CREATE 2
#define MSG_WRITE 3
#define MSG_STAT 4
#define MSG_READ 5
#define MSG_UNLINK 6

#include "mfs.h"

typedef struct _client_message {
    int msg_type;

    union {
        struct {
            int pinum;
            char name[MAX_NAME_LEN];
        } lookup;
        struct {
            int pinum;
            int type;
            char name[MAX_NAME_LEN];
        } create;
        struct {
            int inum;
            char buffer[MAX_BUFFER_SIZE];
            int offset;
            int nbytes
        } write;
        struct {
            int inum;
            MFS_Stat_t *m;
        } stat;
        struct {
            int inum;
            char buffer[4096];
            int offset;
            int nbytes
        } read;
        struct {
            int pinum;
            char name[MAX_NAME_LEN];
        } unlink;
    };
} client_message_t;

typedef struct _server_message {
    int return_code;
    
    union {
        MFS_Stat_t stat;
    };
} server_message_t;
#endif
