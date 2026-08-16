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

struct checkpoint_crypto_metrics
{
	uint64_t seal_prep_cycles;
	uint64_t seal_encrypt_cycles;
	uint64_t seal_tag_cycles;
	uint64_t seal_copy_cycles;
	uint64_t unseal_copy_in_cycles;
	uint64_t unseal_tag_cycles;
	uint64_t unseal_compare_cycles;
	uint64_t unseal_decrypt_cycles;
	uint64_t unseal_copy_out_cycles;
};

int derive_checkpoint_material(void);
int seal_checkpoint_blob(struct sealed_checkpoint *blob, const struct checkpoint *checkpoint);
int unseal_checkpoint_blob(struct checkpoint *checkpoint, const struct sealed_checkpoint *blob);
int seal_checkpoint_blob_profile(struct sealed_checkpoint *blob,
								 const struct checkpoint *checkpoint,
								 struct checkpoint_crypto_metrics *metrics);
int unseal_checkpoint_blob_profile(struct checkpoint *checkpoint,
								   const struct sealed_checkpoint *blob,
								   struct checkpoint_crypto_metrics *metrics);

#endif