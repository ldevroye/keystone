#include "checkpoint.h"

#include <stddef.h>
#include <string.h>

#include "edge/edge_common.h"

#include "common.h"
#include "crypto.h"

static struct checkpoint checkpoint_storage;
static struct sealed_checkpoint checkpoint_blob;
struct rewind_state *state_anchor;

int load_checkpoint(int send_edge_call)
{   
    if (send_edge_call)
    {
           // the host returns the sealed blob, not plain checkpoint data
        if (ocall(OCALL_LOAD_CHECKPOINT_BLOB, NULL, 0, &checkpoint_blob, sizeof(checkpoint_blob)) != 0)
        {
            eapp_print("No saved checkpoint");
            return -1;
        }

    }


    if (open_checkpoint_blob(&checkpoint_storage, &checkpoint_blob) != 0)
    {
        return -1;
    }

    checkpoint_storage.checkpoint_seq++;
    return 0;
}

int restore_checkpoint()
{
    if (state_anchor == NULL)
    {
        eapp_print("Invalid checkpoint anchor");
        return -1;
    }

    memcpy(state_anchor,
           checkpoint_storage.stack_data + (STACK_SNAPSHOT_SIZE - sizeof(*state_anchor)),
           sizeof(*state_anchor));

    return 0;
}

int save_checkpoint(int send_edge_call)
{
    uintptr_t stack_sp;
    uintptr_t stack_anchor;
    uintptr_t snapshot_sp;
    uintptr_t snapshot_end;
    size_t snapshot_len;
    struct checkpoint to_save;

    if (state_anchor == NULL)
    {
        eapp_print("Invalid checkpoint anchor");
        return -1;
    }

    memset(&to_save, 0, sizeof(to_save));

    stack_sp = read_stack_pointer();
    stack_anchor = (uintptr_t)state_anchor;
    snapshot_end = stack_anchor + sizeof(*state_anchor);
    snapshot_sp = snapshot_end > STACK_SNAPSHOT_SIZE
        ? snapshot_end - STACK_SNAPSHOT_SIZE
        : stack_sp;

    // keep the snapshot aligned to the current stack so we never copy below sp
    if (snapshot_sp < stack_sp) {snapshot_sp = stack_sp;}

    if (snapshot_end < snapshot_sp)
    {
        eapp_print("Invalid snapshot anchor");
        return -1;
    }

    snapshot_len = snapshot_end - snapshot_sp;

    if (snapshot_len > STACK_SNAPSHOT_SIZE)
    {
        eapp_print("Stack snapshot too large");
        return -1;
    }

    // snapshot the live stack range and right-align it in a fixed-size buffer
    to_save.checkpoint_seq = checkpoint_storage.checkpoint_seq;
    memset(to_save.stack_data, 0, sizeof(to_save.stack_data));
    memcpy(to_save.stack_data + (STACK_SNAPSHOT_SIZE - snapshot_len), (void *)snapshot_sp, snapshot_len);

    memset(&checkpoint_blob, 0, sizeof(checkpoint_blob));

    if (seal_checkpoint_blob(&checkpoint_blob, &to_save) != 0)
    {
        return -1;
    }

    if (send_edge_call)
    {
        if (ocall(OCALL_SAVE_CHECKPOINT_BLOB, &checkpoint_blob, sizeof(checkpoint_blob), NULL, 0) != 0)
        {
            eapp_print("failed to save checkpoint");
            return -1;
        }
    }

    checkpoint_storage.checkpoint_seq++;
    return 0;
}
