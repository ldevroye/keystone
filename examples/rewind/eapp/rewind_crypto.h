#ifndef REWIND_CRYPTO_H
#define REWIND_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include "app/sealing.h"

int derive_checkpoint_key(struct sealing_key *sk);
void xor_blob(uint8_t *data, size_t data_len, const uint8_t *key);

#endif