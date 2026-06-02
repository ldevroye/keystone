#include <stdio.h>
#include <stdlib.h>

#include "app/syscall.h"

enum {
    OCALL_SAVE_COUNTER = 9,
    OCALL_LOAD_COUNTER = 8,
};

/* Provide a local ocall implementation for eapp linking when the
 * common syscall implementation is not linked into the app binary. */
int ocall(unsigned long call_id, void* data, size_t data_len, void* return_buffer, size_t return_len) {
    return SYSCALL_5(RUNTIME_SYSCALL_OCALL, call_id, data, data_len, return_buffer, return_len);
}

void eapp_print(char* str)
{
  printf("[EAPP] %s\n", str);
}


int main() {
    int counter = 0;
    int next_counter = 0;

    if (ocall(OCALL_LOAD_COUNTER, NULL, 0, &counter, sizeof(counter)) != 0) {
        eapp_print("failed to load counter checkpoint");
        __builtin_trap();
    }
    
    eapp_print("Rewind enclave start");

    for (; counter < 10; counter++) {
        char to_prt[50];
        sprintf(to_prt, "counter = %d", counter);
        eapp_print(to_prt);

        next_counter = counter + 1;
        if (ocall(OCALL_SAVE_COUNTER, &next_counter, sizeof(next_counter), NULL, 0) != 0) {
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