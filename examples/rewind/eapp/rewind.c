#include <string.h>

#include "app/syscall.h"
#include "app/eapp_utils.h"
#include "checkpoint.h"
#include "fault.h"

#define OCALL_PRINT_BUFFER 1

#ifndef REWIND_MAX_ITERATIONS
#define REWIND_MAX_ITERATIONS 50
#endif

#ifndef TESTING
#define TESTING 0
#endif

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

static void eapp_print_if_testing(const char* str)
{
#if TESTING
    eapp_print((char*)str);
#else
    (void)str;
#endif
}

static int format_value(char *buf, const int counter, const char* val)
{   
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);
    int pos = pfx_len;

    unsigned int u;
    if (counter == 0) 
    {
        buf[pos++] = '0';
    } else {
        int neg = 0;
        if (counter < 0) 
        {
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

static int format_unsigned_value(char *buf, const unsigned long value, const char* val)
{
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);

    int pos = pfx_len;
    unsigned long u = value;

    if (u == 0)
    {
        buf[pos++] = '0';
    }
    else
    {
        char rev[32];
        int ri = 0;
        while (u) { rev[ri++] = (char)('0' + (u % 10)); u /= 10; }

        for (int j = ri - 1; j >= 0; --j)
        {
            buf[pos++] = rev[j];
        }
    }

    buf[pos] = '\0';
    return pos;
}

static int format_float_value(char *buf, double value, const char *val)
{
    const char* equal = " = ";
    const int val_len = strlen(val);
    const int equal_len = strlen(equal);
    const int pfx_len = val_len + equal_len;

    memcpy(buf, val, val_len);
    memcpy(buf + val_len, equal, equal_len);

    int pos = pfx_len;
    if (value < 0.0) 
    {
        // Preserve the sign separately, then format the magnitude only.
        buf[pos++] = '-';
        value = -value;
    }

    // Split the value into the integer part and the fractional part.
    unsigned long whole = (unsigned long)value;
    double fraction = value - (double)whole;
    // Keep only two decimal digits by scaling and rounding the fraction.
    unsigned long scaled = (unsigned long)(fraction * 100.0 + 0.5);

    // Build the integer digits from right to left because division removes
    // the least-significant digit first.
    char rev[32];
    int ri = 0;
    do 
    {
        rev[ri++] = (char)('0' + (whole % 10));
        whole /= 10;
    } while (whole != 0);

    // Copy the digits back in forward order into the output buffer.
    for (int j = ri - 1; j >= 0; --j) 
    {
        buf[pos++] = rev[j];
    }

    // Append the two decimal digits.
    buf[pos++] = '.';
    buf[pos++] = (char)('0' + (scaled / 10));
    buf[pos++] = (char)('0' + (scaled % 10));
    buf[pos] = '\0';
    return pos;
}


int test_fault() 
{
    struct fault_model fault_model = MODEL_DEFAULT;
    int counter = 0;
    int nb_faults = 0;

    for (; counter < 10000; counter++) 
    {
        if (fault_should_trigger(&fault_model)) 
        {
            nb_faults++;
        }
    }

    char formated_counter[32], formated_fault[32], formated_rate[32];
    double rate = nb_faults == 0 ? 0.0 : (double)counter / (double)nb_faults;
    format_value(formated_counter, counter, "counter");
    format_value(formated_fault, nb_faults, "nb faults");
    format_float_value(formated_rate, rate, "rate");
    eapp_print(formated_counter);
    eapp_print(formated_fault);
    eapp_print(formated_rate); // this should be equal to fault.h/PERIOD

    EAPP_RETURN(0);
}

int main() 
{
    struct rewind_state state = {0, 1, 0}; // fibonacci sequence init
    struct rewind_checkpoint checkpoint; // empty for now as we will try to load the stack into it
    struct fault_model fault_model = get_default_model();

    eapp_print_if_testing("testing mode enabled");

    // on restart, recover the last sealed checkpoint if the host has one
    if (load_checkpoint(&checkpoint) == 0) 
    {
        if (restore_checkpoint(&state, &checkpoint) == 0) 
        {
            eapp_print("loading stack snapshot"); 
        }
    }
    
    eapp_print("Rewind enclave start");

    for (; state.counter < REWIND_MAX_ITERATIONS;) 
    {
        char formated_counter[32], formated_fib[32];
        format_value(formated_counter, state.counter, "counter");
        format_unsigned_value(formated_fib, state.b, "output");
        
        eapp_print_if_testing(formated_counter);
        eapp_print_if_testing(formated_fib);

        // inject one modeled fault point using a simple pseudo-random splitex function
        // fault happens before so that the "computation" can fail
        if (fault_should_trigger(&fault_model)) 
        {
            eapp_print("Simulated fault");
            //asm volatile("unimp"); // returns illegal RISC-V instruction
            __builtin_trap();
            //return 16; // Keystone::Error::EnclaveInterrupted
        }

        unsigned long next = state.a + state.b;
        state.a = state.b;
        state.b = next;
        state.counter++;

        if (save_checkpoint((uintptr_t)&state, sizeof(state)) != 0) 
        {
            eapp_print_if_testing("failed to save stack checkpoint");
            __builtin_trap();
        }
        
    }

    EAPP_RETURN(0);
} 