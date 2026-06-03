#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/syscall.h"
#include "app/sealing.h"
#include "app/eapp_utils.h"
#include "edge/edge_common.h"

enum {
    OCALL_PRINT_BUFFER = 1,
    OCALL_SAVE_COUNTER = 9,
    OCALL_LOAD_COUNTER = 8,
};

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

static int format_counter(char *buf, int counter)
{
    const char *prefix = "counter = ";
    const int pfx_len = 10; /* length of "counter = " */
    memcpy(buf, prefix, pfx_len);
    int pos = pfx_len;

    unsigned int u;
    if (counter == 0) {
        buf[pos++] = '0';
    } else {
        int neg = 0;
        if (counter < 0) { neg = 1; u = (unsigned)(-counter); } else { u = (unsigned)counter; }
        char rev[16];
        int ri = 0;
        while (u) { rev[ri++] = '0' + (u % 10); u /= 10; }
        if (neg) buf[pos++] = '-';
        for (int j = ri - 1; j >= 0; --j) buf[pos++] = rev[j];
    }
    buf[pos] = '\0';
    return pos;
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
            eapp_print("Failed to derive sealing key");
        }
    } else {
        eapp_print("No saved checkpoint");
    }
    
    eapp_print("Rewind enclave start");

    for (; counter < 10; counter++) {
        char to_prt[32];
        format_counter(to_prt, counter);
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

    EAPP_RETURN(0);
} 