#ifndef MESSAGE_H
#define MESSAGE_H

#define MAX_NAME_LEN 28

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
            int type;
            char name[MAX_NAME_LEN];
            char buffer[4096];
        } create;
        struct {
            int inum;
        } stat;
    };
} client_message_t;

typedef struct _server_message {
    int return_code;

    union {
        MFS_Stat_t stat;
    };
} server_message_t;
#endif
