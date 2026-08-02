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
 * 1) derive_checkpoint_material() asks the platform sealing API for key
 *    material bound to scheme label (key_id[]).
 * 2) The returned material is split into two independent AES-256 keys:
 *    - enc_key for confidentiality (CTR stream encryption)
 *    - auth_key for integrity/authenticity (CBC-MAC)
 * 3) compute_tag() MACs the plaintext checkpoint payload.
 * 4) encrypt_stack() AES-CTR-encrypts the plaintext payload in place using the
 *    MAC as a synthetic IV, so the host only sees opaque sealed bytes.
 * 5) The ciphertext and tag are copied into the host-facing blob.
 *
 * Load:
 * 1) Derive the same enc_key/auth_key again from the same sealing context.
 * 2) Split the opaque blob into ciphertext and tag.
 * 3) Decrypt the ciphertext with the tag as the synthetic IV.
 * 4) Recompute the expected tag over the recovered plaintext.
 * 5) Compare expected tag with the stored tag; if mismatch, reject.
 * 6) Rebuild the plain checkpoint view from verified/decrypted data.
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

static int compute_tag(const uint8_t *payload,
                        size_t payload_len,
                        uint8_t tag[CHECKPOINT_TAG_SIZE])
{
    WORD auth_schedule[AES_SCHEDULE_WORDS];
    uint8_t zero_iv[AES_BLOCK_SIZE] = {0};

    if (payload_len % AES_BLOCK_SIZE != 0) {return -1;}

    aes_key_setup(auth_key, auth_schedule, AES_KEY_BITS);
    // mac the plaintext payload so the tag doubles as a synthetic iv
    if (aes_encrypt_cbc_mac((const BYTE *)payload, payload_len, tag, auth_schedule, AES_KEY_BITS, zero_iv) == 0) 
    {
        return -1;
    }

    return 0;
}

int seal_checkpoint_blob(struct sealed_checkpoint *blob, const struct checkpoint *checkpoint)
{
    uint8_t computed_tag[CHECKPOINT_TAG_SIZE];
    uint8_t payload[sizeof(struct checkpoint)];
    uint8_t *ciphertext = blob->sealed;
    uint8_t *tag = blob->sealed + sizeof(payload);

    memcpy(payload, checkpoint, sizeof(payload));

    // save path: authenticate the plaintext payload, then encrypt it with the tag as iv
    if (derive_checkpoint_material() != 0) {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    if (compute_tag(payload, sizeof(payload), computed_tag) != 0) {
        eapp_print("failed to authenticate checkpoint");
        return -1;
    }

    encrypt_stack(payload, sizeof(payload), computed_tag);

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

    // load path: decrypt with the tag as iv, then verify the recovered plaintext
    if (derive_checkpoint_material() != 0) 
    {
        eapp_print("failed to derive sealing key");
        return -1;
    }

    memcpy(payload, ciphertext, sizeof(payload));
    decrypt_stack(payload, sizeof(payload), tag);

    if (compute_tag(payload, sizeof(payload), expected_tag) != 0) 
    {
        eapp_print("failed to verify checkpoint");
        return -1;
    }

    if (memcmp(expected_tag, tag, sizeof(expected_tag)) != 0) 
    {
        eapp_print("checkpoint authentication failed");
        return -1;
    }

    memcpy(checkpoint, payload, sizeof(*checkpoint));
    return 0;
}
