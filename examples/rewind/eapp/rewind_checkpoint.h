#ifndef REWIND_CHECKPOINT_H
#define REWIND_CHECKPOINT_H

#include <stddef.h>
#include <stdint.h>

#define STACK_SNAPSHOT_SIZE 8*1024
#define OCALL_SAVE_COUNTER 9
#define OCALL_LOAD_COUNTER 8

struct rewind_state {
    int a;
    int b;
    int counter;
};

struct rewind_checkpoint {
    uintptr_t stack_sp;
    uintptr_t stack_fp;
    size_t stack_len;
    uint8_t stack_data[STACK_SNAPSHOT_SIZE];
};

void eapp_print(char *str);

int load_checkpoint(struct rewind_checkpoint *checkpoint);
int restore_checkpoint(struct rewind_state *state, const struct rewind_checkpoint *checkpoint);
int save_checkpoint(uintptr_t stack_anchor, size_t anchor_len);

#endif