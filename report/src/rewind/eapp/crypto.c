#include <stddef.h>
#include <string.h>

#include "app/eapp_utils.h"
#include "app/sealing.h"
#include "crypto/aes.h"

#include "checkpoint.h"
#include "crypto.h"

/*
 * Workflow:
 *
 * Save:
 * 1) derive_checkpoint_material() asks the platform sealing API for key
 *    material bound to scheme label (key_id[]).
 * 2) The returned material is split into two independent AES-256 keys:
 *    - enc_key for confidentiality (CTR stream encryption)
 *    - auth_key for integrity/authenticity (CBC-MAC)
 * 3) derive_nonce() builds a 16-byte nonce from a fixed prefix and the
 *    checkpoint sequence number so each checkpoint uses a distinct CTR stream.
 * 4) encrypt_stack() AES-CTR-encrypts blob->stack_data in place.
 * 5) compute_tag() MACs the whole blob prefix up to (but excluding) tag,
 *    covering metadata + nonce + ciphertext so tampering is detectable.
 * 6) The computed tag is copied into blob->tag before sending the blob out.
 *
 * Load:
 * 1) Derive the same enc_key/auth_key again from the same sealing context.
 * 2) Recompute the expected tag over the received blob contents.
 * 3) Compare expected tag with blob->tag; if mismatch, reject immediately.
 * 4) if success, decrypt blob->stack_data in place.
 * 5) Rebuild the plain rewind_checkpoint view from verified/decrypted data.
 */


// 'static' for interal usage only
static int derive_checkpoint_material(struct sealing_key *sk, 
                                        uint8_t enc_key[AES_KEY_SIZE], 
                                        uint8_t auth_key[AES_KEY_SIZE])
{
    const char key_id[] = "rewind-checkpoint-v1";

    // reuse the sealing root key, then split it into separate enc and auth keys
    if (get_sealing_key(sk, sizeof(*sk), (void *)key_id, sizeof(key_id) - 1) != 0) 
    {
        return -1;
    }

    memcpy(enc_key, sk->key, AES_KEY_SIZE);
    memcpy(auth_key, sk->key + AES_KEY_SIZE, AES_KEY_SIZE);
    return 0;
}

static void derive_nonce(uint8_t nonce[CHECKPOINT_NONCE_SIZE], uint64_t sequence)
{
    static const uint8_t nonce_prefix[8] = {'R', 'W', 'N', 'D', 'C', 'K', 'P', 'T'};

    // prefix the nonce with a fixed label and append the checkpoint sequence
    memset(nonce, 0, CHECKPOINT_NONCE_SIZE);
    memcpy(nonce, nonce_prefix, sizeof(nonce_prefix));

    for (size_t i = 0; i < sizeof(sequence); ++i) {
        nonce[CHECKPOINT_NONCE_SIZE - 1 - i] = (uint8_t)(sequence >> (i * 8));
    }
}

static int encrypt_stack(uint8_t *stack_data,
                            size_t stack_len,
                            const uint8_t enc_key[AES_KEY_SIZE],
                            const uint8_t nonce[CHECKPOINT_NONCE_SIZE])
{
    WORD enc_schedule[AES_SCHEDULE_WORDS];

    aes_key_setup(enc_key, enc_schedule, AES_KEY_BITS);
    aes_encrypt_ctr(stack_data, stack_len, stack_data, enc_schedule, AES_KEY_BITS, nonce);
    return 0;
}

static int decrypt_stack(uint8_t *stack_data,
                            size_t stack_len,
                            const uint8_t enc_key[AES_KEY_SIZE],
                            const uint8_t nonce[CHECKPOINT_NONCE_SIZE])
{
    WORD enc_schedule[AES_SCHEDULE_WORDS];

    aes_key_setup(enc_key, enc_schedule, AES_KEY_BITS);
    aes_decrypt_ctr(stack_data, stack_len, stack_data, enc_schedule, AES_KEY_BITS, nonce);
    return 0;
}

static int compute_tag(const struct rewind_checkpoint_blob *blob,
                        const uint8_t auth_key[AES_KEY_SIZE],
                        uint8_t tag[CHECKPOINT_TAG_SIZE])
{
    WORD auth_schedule[AES_SCHEDULE_WORDS];
    uint8_t zero_iv[AES_BLOCK_SIZE] = {0};
    size_t auth_len = offsetof(struct rewind_checkpoint_blob, tag);

    if (auth_len % AES_BLOCK_SIZE != 0) 
    {
        return -1;
    }

    aes_key_setup(auth_key, auth_schedule, AES_KEY_BITS);
    // mac the metadata and ciphertext together so any tampering is detected
    if (aes_encrypt_cbc_mac((const BYTE *)blob, auth_len, tag, auth_schedule, AES_KEY_BITS, zero_iv) == 0) 
    {
        return -1;
    }

    return 0;
}

int seal_checkpoint_blob(struct rewind_checkpoint_blob *blob)
{
    struct sealing_key sk;
    uint8_t enc_key[AES_KEY_SIZE];
    uint8_t auth_key[AES_KEY_SIZE];
    uint8_t computed_tag[CHECKPOINT_TAG_SIZE];

    // save path: encrypt the stack bytes, authenticate the whole blob, then hand it to the host
    if (derive_checkpoint_material(&sk, enc_key, auth_key) != 0) {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    derive_nonce(blob->nonce, blob->checkpoint_seq);
    encrypt_stack(blob->stack_data, STACK_SNAPSHOT_SIZE, enc_key, blob->nonce);

    if (compute_tag(blob, auth_key, computed_tag) != 0) {
        eapp_print("failed to authenticate checkpoint");
        return -1;
    }

    memcpy(blob->tag, computed_tag, sizeof(computed_tag));
    return 0;
}

int open_checkpoint_blob(struct rewind_checkpoint *checkpoint, struct rewind_checkpoint_blob *blob)
{
    struct sealing_key sk;
    uint8_t enc_key[AES_KEY_SIZE];
    uint8_t auth_key[AES_KEY_SIZE];
    uint8_t expected_tag[CHECKPOINT_TAG_SIZE];

    // load path: verify first, then decrypt and rebuild the plain checkpoint view
    if (derive_checkpoint_material(&sk, enc_key, auth_key) != 0) 
    {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    if (compute_tag(blob, auth_key, expected_tag) != 0) 
    {
        eapp_print("failed to verify checkpoint");
        return -1;
    }

    if (memcmp(expected_tag, blob->tag, sizeof(expected_tag)) != 0) 
    {
        eapp_print("checkpoint authentication failed");
        return -1;
    }

    decrypt_stack(blob->stack_data, STACK_SNAPSHOT_SIZE, enc_key, blob->nonce);

    checkpoint->stack_sp = blob->stack_sp;
    checkpoint->stack_fp = blob->stack_fp;
    checkpoint->stack_len = blob->stack_len;
    memcpy(checkpoint->stack_data, blob->stack_data, sizeof(checkpoint->stack_data));
    return 0;
}
