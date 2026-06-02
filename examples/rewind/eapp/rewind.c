#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/syscall.h"
#include "app/sealing.h"

enum {
    OCALL_PRINT_BUFFER = 1,
    OCALL_SAVE_COUNTER = 9,
    OCALL_LOAD_COUNTER = 8,
};

/* Provide a local ocall implementation for eapp linking when the
 * common syscall implementation is not linked into the app binary. */
int ocall(unsigned long call_id, void* data, size_t data_len, void* return_buffer, size_t return_len) {
    return SYSCALL_5(RUNTIME_SYSCALL_OCALL, call_id, data, data_len, return_buffer, return_len);
}

/* Local wrapper for get_sealing_key to avoid depending on external syscall
 * implementation being linked into the eapp binary. */
int get_sealing_key(
    struct sealing_key* sealing_key_struct, size_t sealing_key_struct_size,
    void* key_ident, size_t key_ident_size) {
  return SYSCALL_4(RUNTIME_SYSCALL_GET_SEALING_KEY,
      sealing_key_struct, sealing_key_struct_size,
      key_ident, key_ident_size);
}

unsigned long ocall_print_buffer(char* data, size_t data_len)
{
    unsigned long retval;

    ocall(OCALL_PRINT_BUFFER, data, data_len, &retval, sizeof(unsigned long));

    return retval;
}

void eapp_print(char* str)
{
    ocall_print_buffer("[EAPP] ", 7);
    ocall_print_buffer(str, strlen(str));
    ocall_print_buffer("\n", 1);
}


int main() {
    int counter = 0;
    int next_counter = 0;
    struct sealing_key sk;
    const char key_id[] = "rewind-checkpoint-v1";
    uint8_t sealed_buf[sizeof(int)];
    size_t sealed_len = sizeof(sealed_buf);

    /* Try to load sealed checkpoint from host and unseal it. */
    if (ocall(OCALL_LOAD_COUNTER, NULL, 0, sealed_buf, sealed_len) == 0) {
        /* derive sealing key and unseal (XOR with first bytes of sealing key) */
        if (get_sealing_key(&sk, sizeof(sk), (void*)key_id, sizeof(key_id) - 1) == 0) {
            uint8_t *k = (uint8_t *)sk.key;
            uint8_t plain[sizeof(int)];
            for (size_t i = 0; i < sizeof(int); i++) {
                plain[i] = sealed_buf[i] ^ k[i];
            }
            memcpy(&counter, plain, sizeof(counter));
        } else {
            eapp_print("failed to derive sealing key");
        }
    } else {
        eapp_print("no saved checkpoint, starting from 0");
    }
    
    eapp_print("Rewind enclave start");

    for (; counter < 10; counter++) {
        char to_prt[50];
        sprintf(to_prt, "counter = %d", counter);
        eapp_print(to_prt);

        next_counter = counter + 1;
        /* seal next_counter using the sealing key (simple XOR with key bytes) */
        if (get_sealing_key(&sk, sizeof(sk), (void*)key_id, sizeof(key_id) - 1) != 0) {
            eapp_print("failed to derive sealing key for save");
            __builtin_trap();
        }
        uint8_t *k = (uint8_t *)sk.key;
        for (size_t i = 0; i < sizeof(int); i++) {
            sealed_buf[i] = ((uint8_t*)&next_counter)[i] ^ k[i];
        }
        if (ocall(OCALL_SAVE_COUNTER, sealed_buf, sealed_len, NULL, 0) != 0) {
            eapp_print("failed to save counter checkpoint");
            __builtin_trap();
        }

        if (counter == 6) {
            eapp_print("Simulated crash");
            //asm volatile("unimp"); // returns illegal RISC-V instruction
            __builtin_trap();
            //return 16; // Keystone::Error::EnclaveInterrupted
        }
    }
    return 0;
} 