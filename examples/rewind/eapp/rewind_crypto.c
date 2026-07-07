#include "rewind_crypto.h"

#include "app/eapp_utils.h"

int derive_checkpoint_key(struct sealing_key *sk)
{
    const char key_id[] = "rewind-checkpoint-v1";

    return get_sealing_key(sk, sizeof(*sk), (void *)key_id, sizeof(key_id) - 1) == 0 ? 0 : -1;
}

void xor_blob(uint8_t *data, size_t data_len, const uint8_t *key)
{
    for (size_t i = 0; i < data_len; ++i) {
        data[i] ^= key[i % SEALING_KEY_SIZE];
    }
}