#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/syscall.h"
#include "app/sealing.h"
#include "app/eapp_utils.h"
#include "edge/edge_common.h"

// Bound the live stack window we keep in the checkpoint blob.
#define REWIND_STACK_SNAPSHOT_SIZE (8 * 1024)

enum {
    OCALL_PRINT_BUFFER = 1,
    OCALL_SAVE_COUNTER = 9,
    OCALL_LOAD_COUNTER = 8,
};

struct rewind_checkpoint {
    uintptr_t stack_sp;
    uintptr_t stack_fp;
    size_t stack_len;
    uint8_t stack_data[REWIND_STACK_SNAPSHOT_SIZE];
};

// Keep state together for the stack snapshot
struct saved_state {
    int a;
    int b;
    int counter;
};

static struct rewind_checkpoint checkpoint_storage;
static uint8_t checkpoint_blob[sizeof(struct rewind_checkpoint)];

unsigned long ocall_print_buffer(char* data, size_t data_len)
{
    unsigned long retval;

    ocall(OCALL_PRINT_BUFFER, data, data_len, &retval, sizeof(unsigned long));

    return retval;
}

void eapp_print(char* str)
{
    ocall_print_buffer("[EAPP] ", 7);
    ocall_print_buffer(str, strlen(str));
    ocall_print_buffer("\n", 1);
}

static int format_value(char *buf,const int counter, const char* val)
{   
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);
    int pos = pfx_len;

    unsigned int u;
    if (counter == 0) {
        buf[pos++] = '0';
    } else {
        int neg = 0;
        if (counter < 0) {
            // Record the sign separately, then convert the magnitude only.
            neg = 1;
            u = (unsigned)(-counter);
        } else {
            u = (unsigned)counter;
        }

        /* Build the decimal digits in reverse order, from least significant
         * digit to most significant digit. This is easier because the modulo
         * and division operations peel one digit at a time from the right. */
        char rev[16];
        int ri = 0;
        while (u) { rev[ri++] = '0' + (u % 10); u /= 10; }
        if (neg) buf[pos++] = '-';

        // Copy the reversed digits back into the output in forward order.
        for (int j = ri - 1; j >= 0; --j) buf[pos++] = rev[j];
    }
    buf[pos] = '\0';
    return pos;
}

static uintptr_t read_stack_pointer(void)
{
    uintptr_t sp;

    /* mv - register move instruction.
     * %0 - output placeholder for the C variable sp.
     * sp - the RISC-V Stack-Pointer register.
     * "=r" is a constraint for GCC (compiler) so that the output is placed in a general 
     * purpose register and store that register's value into sp.
     */
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return sp;
}

static int derive_checkpoint_key(struct sealing_key *sk)
{
    const char key_id[] = "rewind-checkpoint-v1";

    // Derive a per-enclave key so the blob stays tied to this identity.
    if (get_sealing_key(sk, sizeof(*sk), (void *)key_id, sizeof(key_id) - 1) != 0) {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    return 0;
}

static void xor_blob(uint8_t *data, size_t data_len, const uint8_t *key)
{
    for (size_t i = 0; i < data_len; ++i) {
        data[i] ^= key[i % SEALING_KEY_SIZE];
    }
}

static int load_checkpoint(struct rewind_checkpoint *checkpoint)
{
    struct sealing_key sk;

    // Host returns the last opaque blob into the shared edge buffer. 
    if (ocall(OCALL_LOAD_COUNTER, NULL, 0, checkpoint_blob, sizeof(checkpoint_blob)) != 0) {
        eapp_print("No saved checkpoint");
        return -1;
    }

    if (derive_checkpoint_key(&sk) != 0) {
        return -1;
    }

    // Unseal the blob with the key before restoring it.
    xor_blob(checkpoint_blob, sizeof(checkpoint_blob), sk.key);
    memcpy(checkpoint, checkpoint_blob, sizeof(*checkpoint));

    if (checkpoint->stack_len > REWIND_STACK_SNAPSHOT_SIZE) {
        eapp_print("Invalid checkpoint stack size");
        return -1;
    }

    return 0;
}

static int restore_checkpoint(const struct rewind_checkpoint *checkpoint)
{
    // Sanity-check the metadata before copying the saved stack bytes back.
    if (checkpoint->stack_len == 0 ||
        checkpoint->stack_len > REWIND_STACK_SNAPSHOT_SIZE ||
        checkpoint->stack_fp <= checkpoint->stack_sp) {
        eapp_print("Invalid checkpoint metadata");
        return -1;
    }

    // Restore the saved live stack window back to its original addresses
    memcpy((void *)checkpoint->stack_sp, checkpoint->stack_data, checkpoint->stack_len);

    return 0;
}

static int save_checkpoint(uintptr_t stack_anchor, size_t anchor_len)
{
    struct sealing_key sk;
    uintptr_t stack_sp;
    uintptr_t snapshot_sp;
    uintptr_t snapshot_end;
    size_t snapshot_len;

    memset(&checkpoint_storage, 0, sizeof(checkpoint_storage));

    stack_sp = read_stack_pointer();

    // Keep the checkpoint window centered around the event anchor
    snapshot_end = stack_anchor + anchor_len;
    snapshot_sp = snapshot_end > REWIND_STACK_SNAPSHOT_SIZE
        ? snapshot_end - REWIND_STACK_SNAPSHOT_SIZE
        : stack_sp;

    if (snapshot_sp < stack_sp) {
        snapshot_sp = stack_sp;
    }

    if (snapshot_end < snapshot_sp) {
        eapp_print("Invalid snapshot anchor");
        return -1;
    }

    // Save only live slice that covers the current event state.
    snapshot_len = snapshot_end - snapshot_sp;

    checkpoint_storage.stack_sp = snapshot_sp;
    checkpoint_storage.stack_fp = snapshot_end;
    checkpoint_storage.stack_len = snapshot_len;

    if (checkpoint_storage.stack_len > REWIND_STACK_SNAPSHOT_SIZE) {
        eapp_print("Stack snapshot too large");
        return -1;
    }

    memcpy(checkpoint_storage.stack_data, (void *)snapshot_sp, checkpoint_storage.stack_len);
    memcpy(checkpoint_blob, &checkpoint_storage, sizeof(checkpoint_storage));

    if (derive_checkpoint_key(&sk) != 0) {
        return -1;
    }

    xor_blob(checkpoint_blob, sizeof(checkpoint_blob), sk.key);

    // save the XORed blob to the host (checkpointing)
    if (ocall(OCALL_SAVE_COUNTER, checkpoint_blob, sizeof(checkpoint_blob), NULL, 0) != 0) {
        eapp_print("failed to save checkpoint");
        return -1;
    }

    return 0;
}


int main() {
    volatile struct saved_state state = {0, 1, 0};
    struct rewind_checkpoint checkpoint;

    // Try to load sealed checkpoint from host and then unseal it.
    if (load_checkpoint(&checkpoint) == 0) {
        // Making sure one is done after the other

        // IF statement is there solely for logging purpose
        if (restore_checkpoint(&checkpoint) == 0) {
            eapp_print("loading stack snapshot"); 
        }
    }
    
    eapp_print("Rewind enclave start");

    for (; state.counter < 10; ) {
        char formated_counter[32], formated_fib[32];
        format_value(formated_counter, state.counter, "counter");
        format_value(formated_fib, state.b, "output");
        
        eapp_print(formated_counter);
        eapp_print(formated_fib);

        int next = state.a + state.b;
        state.a = state.b;
        state.b = next;
        state.counter++;

        if (save_checkpoint((uintptr_t)&state, sizeof(state)) != 0) {
            eapp_print("failed to save stack checkpoint");
            __builtin_trap();
        }

        if (state.counter == 7) {
            eapp_print("Simulated crash");
            //asm volatile("unimp"); // returns illegal RISC-V instruction
            __builtin_trap();
            //return 16; // Keystone::Error::EnclaveInterrupted
        }
    }

    EAPP_RETURN(0);
} 