#include <stddef.h>
#include <string.h>

#include "app/eapp_utils.h"
#include "app/sealing.h"
#include "crypto/aes.h"

#include "checkpoint.h"
#include "crypto.h"

static const char ENC_KEY_IDENTIFIER[] = "rewind-checkpoint-enc-v1";
static const char AUTH_KEY_IDENTIFIER[] = "rewind-checkpoint-auth-v1";

static uint8_t enc_key[AES_KEY_SIZE];
static uint8_t auth_key[AES_KEY_SIZE];
static int checkpoint_keys_ready;

/*
 * Workflow:
 *
 * Save:
 * a) derive_checkpoint_material() asks the SM sealing API for key
 *    material bound to scheme label (identifiers[]).
 * b) The returned material is two independent AES-256 keys:
 *    - enc_key for confidentiality (CTR stream encryption)
 *    - auth_key for integrity/authenticity (CBC-MAC)
 * c) encrypt_stack(payload) AES-CTR-encrypts the plaintext payload
 *    with the checkpoint iv.
 * d) compute_tag() MACs the iv and ciphertext together.
 * e) The iv, payload, and tag are copied into the host-facing blob.
 *
 * Load:
 * a) Derive the same enc_key/auth_key again from the same context.
 * b) Split the opaque blob into ciphertext, iv, and tag.
 * c) Recompute the expected tag over the iv and ciphertext.
 * d) Compare expected tag with the stored tag; if mismatch, reject.
 * e) Decrypt the ciphertext with the authenticated iv.
 * f) Rebuild the plain checkpoint.
*/


// 'static' for interal usage only
static int derive_checkpoint_material(void)
{
    struct sealing_key sk;

    if (checkpoint_keys_ready) {return 0;}

    // separate labels provide domain-separated keys instead of splitting one kdf output
    if (get_sealing_key(&sk,
                        sizeof(sk),
                        (void *)ENC_KEY_IDENTIFIER,
                        sizeof(ENC_KEY_IDENTIFIER) - 1) != 0)
    {
        return -1;
    }
    memcpy(enc_key, sk.key, AES_KEY_SIZE);

    if (get_sealing_key(&sk,
                        sizeof(sk),
                        (void *)AUTH_KEY_IDENTIFIER,
                        sizeof(AUTH_KEY_IDENTIFIER) - 1) != 0)
    {
        return -1;
    }
    memcpy(auth_key, sk.key, AES_KEY_SIZE);

    checkpoint_keys_ready = 1;
    return 0;
}

static int encrypt_stack(uint8_t *stack_data,
                            size_t stack_len,
                            const uint8_t nonce[AES_BLOCK_SIZE])
{
    WORD enc_schedule[AES_SCHEDULE_WORDS];

    aes_key_setup(enc_key, enc_schedule, AES_KEY_BITS);
    aes_encrypt_ctr(stack_data, stack_len, stack_data, enc_schedule, AES_KEY_BITS, nonce);
    return 0;
}

static int decrypt_stack(uint8_t *stack_data,
                            size_t stack_len,
                            const uint8_t nonce[AES_BLOCK_SIZE])
{
    WORD enc_schedule[AES_SCHEDULE_WORDS];

    aes_key_setup(enc_key, enc_schedule, AES_KEY_BITS);
    aes_decrypt_ctr(stack_data, stack_len, stack_data, enc_schedule, AES_KEY_BITS, nonce);
    return 0;
}

static int constant_time_equal(const uint8_t *left,
                               const uint8_t *right,
                               size_t len)
{
    uint8_t diff = 0;

    for (size_t idx = 0; idx < len; idx++)
    {
        diff |= left[idx] ^ right[idx];
    }

    return diff == 0;
}

static int compute_tag(const uint8_t iv[AES_BLOCK_SIZE],
                        const uint8_t *payload,
                        size_t payload_len,
                        uint8_t tag[CHECKPOINT_TAG_SIZE])
{
    WORD auth_schedule[AES_SCHEDULE_WORDS];
    uint8_t zero_iv[AES_BLOCK_SIZE] = {0};
    uint8_t mac_input[AES_BLOCK_SIZE + sizeof(struct checkpoint)];
    size_t mac_len = AES_BLOCK_SIZE + payload_len;

    if (payload_len % AES_BLOCK_SIZE != 0) {return -1;}

    aes_key_setup(auth_key, auth_schedule, AES_KEY_BITS);
    memcpy(mac_input, iv, AES_BLOCK_SIZE);
    memcpy(mac_input + AES_BLOCK_SIZE, payload, payload_len);

    // mac the iv and ciphertext together so the decrypt nonce is authenticated too
    if (aes_encrypt_cbc_mac((const BYTE *)mac_input, mac_len, tag, auth_schedule, AES_KEY_BITS, zero_iv) == 0) 
    {
        return -1;
    }

    return 0;
}

int seal_checkpoint_blob(struct sealed_checkpoint *blob, const struct checkpoint *checkpoint)
{
    uint8_t computed_tag[CHECKPOINT_TAG_SIZE];
    uint8_t payload[sizeof(struct checkpoint)];
    uint8_t iv[AES_BLOCK_SIZE] = {0};
    uint8_t *ciphertext = blob->sealed;
    uint8_t *tag = blob->sealed + sizeof(payload);

    memcpy(payload, checkpoint, sizeof(payload));

    // checkpoint_seq must never repeat across the enclave lifetime for iv uniqueness to hold
    memcpy(iv, &checkpoint->checkpoint_seq, sizeof(checkpoint->checkpoint_seq));

    // removes empty 0s as the checkpoint_seq is AES_BLOCK_SIZE//2 (16//2 -> 8) bytes
    memcpy(iv + sizeof(checkpoint->checkpoint_seq), &checkpoint->checkpoint_seq, sizeof(checkpoint->checkpoint_seq));

    // save path: encrypt first, then authenticate the iv and ciphertext
    if (derive_checkpoint_material() != 0) {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    encrypt_stack(payload, sizeof(payload), iv);

    if (compute_tag(iv, payload, sizeof(payload), computed_tag) != 0) {
        eapp_print("failed to authenticate checkpoint");
        return -1;
    }

    memcpy(blob->iv, iv, sizeof(iv));
    memcpy(ciphertext, payload, sizeof(payload));
    memcpy(tag, computed_tag, sizeof(computed_tag));

    return 0;
}

int open_checkpoint_blob(struct checkpoint *checkpoint, const struct sealed_checkpoint *blob)
{
    uint8_t expected_tag[CHECKPOINT_TAG_SIZE];
    uint8_t payload[sizeof(struct checkpoint)];
    const uint8_t *ciphertext = blob->sealed;
    const uint8_t *tag = blob->sealed + sizeof(payload);

    // load path: verify the iv and ciphertext first, then decrypt
    if (derive_checkpoint_material() != 0) 
    {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    memcpy(payload, ciphertext, sizeof(payload));

    if (compute_tag(blob->iv, payload, sizeof(payload), expected_tag) != 0) 
    {
        eapp_print("failed to verify checkpoint");
        return -1;
    }

    if (!constant_time_equal(expected_tag, tag, sizeof(expected_tag))) 
    {
        eapp_print("checkpoint authentication failed");
        return -1;
    }

    decrypt_stack(payload, sizeof(payload), blob->iv);

    memcpy(checkpoint, payload, sizeof(*checkpoint));
    return 0;
}
