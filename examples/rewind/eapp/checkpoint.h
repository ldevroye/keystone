#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <stddef.h>
#include <stdint.h>

#define STACK_SNAPSHOT_SIZE 8*1024
#define OCALL_SAVE_CHECKPOINT_BLOB 9
#define OCALL_LOAD_CHECKPOINT_BLOB 8

#define CHECKPOINT_NONCE_SIZE 16
#define CHECKPOINT_TAG_SIZE 16

struct rewind_state {
    int a;
    int b;
    int counter;
};

// "Safe" checkpoint - interface for the sealing blob used by enclave (plain data)
struct rewind_checkpoint {
    uintptr_t stack_sp;
    uintptr_t stack_fp;
    size_t stack_len;
    uint8_t stack_data[STACK_SNAPSHOT_SIZE];
};

// Complete struct with blob (opaque data)
struct rewind_checkpoint_blob {
    uintptr_t stack_sp;
    uintptr_t stack_fp;
    size_t stack_len;
    uint64_t checkpoint_seq;
    uint8_t nonce[CHECKPOINT_NONCE_SIZE];
    uint8_t reserved[16];
    uint8_t stack_data[STACK_SNAPSHOT_SIZE];
    uint8_t tag[CHECKPOINT_TAG_SIZE];
};

void eapp_print(char *str);

int load_checkpoint(struct rewind_checkpoint *checkpoint);
int restore_checkpoint(struct rewind_state *state, const struct rewind_checkpoint *checkpoint);
int save_checkpoint(uintptr_t stack_anchor, size_t anchor_len);

#endif