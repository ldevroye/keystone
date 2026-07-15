#include "checkpoint.h"

#include <string.h>
#include <stddef.h>

#include "app/eapp_utils.h"
#include "edge/edge_common.h"

#include "crypto.h"


static struct rewind_checkpoint checkpoint_storage;
static struct rewind_checkpoint_blob checkpoint_blob;
static uint64_t checkpoint_sequence;

static uintptr_t read_stack_pointer(void)
{
    uintptr_t sp;

    /* mv - register move instruction
     * %0 - output placeholder for the C variable sp
     * sp - the RISC-V Stack-Pointer register
     * "=r" - a constraint for GCC (compiler) so that the output is placed in a general- 
     * -purpose register and store that register's value into sp
    */
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return sp;
}

int load_checkpoint(struct rewind_checkpoint *checkpoint)
{
    // the host returns the sealed blob, not plain checkpoint data
    if (ocall(OCALL_LOAD_CHECKPOINT_BLOB, NULL, 0, &checkpoint_blob, sizeof(checkpoint_blob)) != 0) 
    {
        eapp_print("No saved checkpoint");
        return -1;
    }

    if (open_checkpoint_blob(checkpoint, &checkpoint_blob) != 0) 
    {
        return -1;
    }

    checkpoint_sequence = checkpoint_blob.checkpoint_seq + 1;

    if (checkpoint->stack_len > STACK_SNAPSHOT_SIZE) 
    {
        eapp_print("Invalid checkpoint stack size");
        return -1;
    }

    return 0;
}

int restore_checkpoint(struct rewind_state *state, const struct rewind_checkpoint *checkpoint)
{
    // the demo only restores the state object that was deliberately kept on the stack
    if (checkpoint->stack_len < sizeof(*state) ||
        checkpoint->stack_len > STACK_SNAPSHOT_SIZE ||
        checkpoint->stack_fp <= checkpoint->stack_sp) 
    {
        eapp_print("Invalid checkpoint metadata");
        return -1;
    }

    memcpy(state, checkpoint->stack_data + (checkpoint->stack_len - sizeof(*state)), sizeof(*state));

    return 0;
}

int save_checkpoint(uintptr_t stack_anchor, size_t anchor_len)
{
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

    // keep the snapshot aligned to the current stack so we never copy below sp
    if (snapshot_sp < stack_sp) 
    {
        snapshot_sp = stack_sp;
    }

    if (snapshot_end < snapshot_sp) 
    {
        eapp_print("Invalid snapshot anchor");
        return -1;
    }

    snapshot_len = snapshot_end - snapshot_sp;

    // snapshot the live stack range that contains the saved_state object
    // then wrap it into a sealed blob before sending it to the host
    checkpoint_storage.stack_sp = snapshot_sp;
    checkpoint_storage.stack_fp = snapshot_end;
    checkpoint_storage.stack_len = snapshot_len;

    // copy the live stack bytes before sealing them for host storage
    if (checkpoint_storage.stack_len > STACK_SNAPSHOT_SIZE) 
    {
        eapp_print("Stack snapshot too large");
        return -1;
    }

    memcpy(checkpoint_storage.stack_data, (void *)snapshot_sp, checkpoint_storage.stack_len);

    memset(&checkpoint_blob, 0, sizeof(checkpoint_blob));
    checkpoint_blob.stack_sp = checkpoint_storage.stack_sp;
    checkpoint_blob.stack_fp = checkpoint_storage.stack_fp;
    checkpoint_blob.stack_len = checkpoint_storage.stack_len;
    checkpoint_blob.checkpoint_seq = checkpoint_sequence;
    memcpy(checkpoint_blob.stack_data, checkpoint_storage.stack_data, sizeof(checkpoint_storage.stack_data));

    if (seal_checkpoint_blob(&checkpoint_blob) != 0) 
    {
        return -1;
    }

    if (ocall(OCALL_SAVE_CHECKPOINT_BLOB, &checkpoint_blob, sizeof(checkpoint_blob), NULL, 0) != 0) 
    {
        eapp_print("failed to save checkpoint");
        return -1;
    }

    checkpoint_sequence++;

    return 0;
}