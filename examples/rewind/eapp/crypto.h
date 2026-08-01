#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>
#include <stdint.h>


#define AES_KEY_SIZE 32
#define AES_KEY_BITS 256
#define AES_BLOCK_SIZE 16
#define AES_SCHEDULE_WORDS 60


// placeholders for the checkpoint.h structs
struct checkpoint; 
struct sealed_checkpoint;

int seal_checkpoint_blob(struct sealed_checkpoint *blob, const struct checkpoint *checkpoint);
int open_checkpoint_blob(struct checkpoint *checkpoint, const struct sealed_checkpoint *blob);

#endif