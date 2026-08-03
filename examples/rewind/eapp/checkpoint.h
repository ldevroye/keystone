#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <stddef.h>
#include <stdint.h>

#include "crypto.h"

// fixed upper bound for the live stack window, independent of rewind_state size
#define STACK_SNAPSHOT_SIZE 8*1024
#define OCALL_SAVE_CHECKPOINT_BLOB 9
#define OCALL_LOAD_CHECKPOINT_BLOB 8

#define CHECKPOINT_TAG_SIZE 16
#define CHECKPOINT_IV_SIZE AES_BLOCK_SIZE
#define CHECKPOINT_SEALED_SIZE (sizeof(struct checkpoint) + CHECKPOINT_TAG_SIZE)
#define CHECKPOINT_BLOB_SIZE (CHECKPOINT_IV_SIZE + CHECKPOINT_SEALED_SIZE)


extern struct rewind_state *checkpoint_state_anchor;

struct rewind_state 
{
    unsigned long a;
    unsigned long b;
    int counter;
};

// plaintext checkpoint state kept inside the enclave
struct checkpoint 
{
    uint64_t checkpoint_seq;
    uint64_t reserved;  //placeholder for the checkpoint to be a multiple of 16 (CBC-MAC restriction)
    uint8_t stack_data[STACK_SNAPSHOT_SIZE];
};

// host-facing sealed blob: opaque bytes only
struct sealed_checkpoint 
{
    uint8_t iv[CHECKPOINT_IV_SIZE];
    uint8_t sealed[CHECKPOINT_SEALED_SIZE];
};

void eapp_print(const char* str); // placeholder

int load_checkpoint();
int restore_checkpoint();
int save_checkpoint();

#endif