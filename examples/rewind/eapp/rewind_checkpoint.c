#include "rewind_checkpoint.h"

#include <string.h>

#include "app/eapp_utils.h"
#include "edge/edge_common.h"

#include "rewind_crypto.h"


static struct rewind_checkpoint checkpoint_storage;
static uint8_t checkpoint_blob[sizeof(struct rewind_checkpoint)];

static uintptr_t read_stack_pointer(void)
{
    uintptr_t sp;

    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return sp;
}

int load_checkpoint(struct rewind_checkpoint *checkpoint)
{
    struct sealing_key sk;

    if (ocall(OCALL_LOAD_COUNTER, NULL, 0, checkpoint_blob, sizeof(checkpoint_blob)) != 0) {
        eapp_print("No saved checkpoint");
        return -1;
    }

    if (derive_checkpoint_key(&sk) != 0) {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    xor_blob(checkpoint_blob, sizeof(checkpoint_blob), sk.key);
    memcpy(checkpoint, checkpoint_blob, sizeof(*checkpoint));

    if (checkpoint->stack_len > STACK_SNAPSHOT_SIZE) {
        eapp_print("Invalid checkpoint stack size");
        return -1;
    }

    return 0;
}

int restore_checkpoint(struct rewind_state *state, const struct rewind_checkpoint *checkpoint)
{
    if (checkpoint->stack_len < sizeof(*state) ||
        checkpoint->stack_len > STACK_SNAPSHOT_SIZE ||
        checkpoint->stack_fp <= checkpoint->stack_sp) {
        eapp_print("Invalid checkpoint metadata");
        return -1;
    }

    memcpy(state, checkpoint->stack_data + (checkpoint->stack_len - sizeof(*state)), sizeof(*state));

    return 0;
}

int save_checkpoint(uintptr_t stack_anchor, size_t anchor_len)
{
    struct sealing_key sk;
    uintptr_t stack_sp;
    uintptr_t snapshot_sp;
    uintptr_t snapshot_end;
    size_t snapshot_len;

    memset(&checkpoint_storage, 0, sizeof(checkpoint_storage));

    stack_sp = read_stack_pointer();

    snapshot_end = stack_anchor + anchor_len;
    snapshot_sp = snapshot_end > STACK_SNAPSHOT_SIZE
        ? snapshot_end - STACK_SNAPSHOT_SIZE
        : stack_sp;

    if (snapshot_sp < stack_sp) {
        snapshot_sp = stack_sp;
    }

    if (snapshot_end < snapshot_sp) {
        eapp_print("Invalid snapshot anchor");
        return -1;
    }

    snapshot_len = snapshot_end - snapshot_sp;

    checkpoint_storage.stack_sp = snapshot_sp;
    checkpoint_storage.stack_fp = snapshot_end;
    checkpoint_storage.stack_len = snapshot_len;

    if (checkpoint_storage.stack_len > STACK_SNAPSHOT_SIZE) {
        eapp_print("Stack snapshot too large");
        return -1;
    }

    memcpy(checkpoint_storage.stack_data, (void *)snapshot_sp, checkpoint_storage.stack_len);
    memcpy(checkpoint_blob, &checkpoint_storage, sizeof(checkpoint_storage));

    if (derive_checkpoint_key(&sk) != 0) {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    xor_blob(checkpoint_blob, sizeof(checkpoint_blob), sk.key);

    if (ocall(OCALL_SAVE_COUNTER, checkpoint_blob, sizeof(checkpoint_blob), NULL, 0) != 0) {
        eapp_print("failed to save checkpoint");
        return -1;
    }

    return 0;
}