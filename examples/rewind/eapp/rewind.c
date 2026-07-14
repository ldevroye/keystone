#include <string.h>

#include "app/syscall.h"
#include "app/eapp_utils.h"
#include "checkpoint.h"

#define OCALL_PRINT_BUFFER 1

struct saved_state {
    int a;
    int b;
    int counter;
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

static int format_value(char *buf,const int counter, const char* val)
{   
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);
    int pos = pfx_len;

    unsigned int u;
    if (counter == 0) {
        buf[pos++] = '0';
    } else {
        int neg = 0;
        if (counter < 0) {
            // record the sign separately, then convert the magnitude only
            neg = 1;
            u = (unsigned)(-counter);
        } else {
            u = (unsigned)counter;
        }

        /* build the decimal digits in reverse order
         * from least significant digit to most significant digit
         * bcs the modulo and division operations operates from the right
        */
        char rev[16];
        int ri = 0;
        while (u) { rev[ri++] = '0' + (u % 10); u /= 10; }
        if (neg) buf[pos++] = '-';

        // copy the reversed digits back into the output in forward order
        for (int j = ri - 1; j >= 0; --j) buf[pos++] = rev[j];
    }
    buf[pos] = '\0';
    return pos;
}


int main() {
    struct saved_state state = {0, 1, 0};
    struct rewind_checkpoint checkpoint;

    // on restart, recover the last sealed checkpoint if the host has one
    if (load_checkpoint(&checkpoint) == 0) {
        if (restore_checkpoint((struct rewind_state *)&state, &checkpoint) == 0) {
            eapp_print("loading stack snapshot"); 
        }
    }
    
    eapp_print("Rewind enclave start");

    for (; state.counter < 10;) {
        char formated_counter[32], formated_fib[32];
        format_value(formated_counter, state.counter, "counter");
        format_value(formated_fib, state.b, "output");
        
        eapp_print(formated_counter);
        eapp_print(formated_fib);

        int next = state.a + state.b;
        state.a = state.b;
        state.b = next;
        state.counter++;

        // save after each step so a crash resumes at the next iteration
        if (save_checkpoint((uintptr_t)&state, sizeof(state)) != 0) {
            eapp_print("failed to save stack checkpoint");
            __builtin_trap();
        }

        if (state.counter == 7) {
            eapp_print("Simulated crash");
            //asm volatile("unimp"); // returns illegal RISC-V instruction
            __builtin_trap();
            //return 16; // Keystone::Error::EnclaveInterrupted
        }
    }

    EAPP_RETURN(0);
} 