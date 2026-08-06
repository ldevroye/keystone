#include "common.h"

#include <string.h>

#include "app/eapp_utils.h"
#include "app/syscall.h"


unsigned long ocall_print_buffer(char* data, size_t data_len)
{
    unsigned long retval;
    ocall(OCALL_PRINT_BUFFER, data, data_len, &retval, sizeof(unsigned long));
    return retval;
}

void eapp_print(const char* str)
{
#if EAPP_LOGGING
    ocall_print_buffer("[EAPP] ", 7);
    ocall_print_buffer(str, strlen(str));
    ocall_print_buffer("\n", 1);
#endif
}

int format_value(char *buf, const int counter, const char* val)
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
    }
    else
    {
        int neg = 0;
        if (counter < 0)
        {
            neg = 1;
            u = (unsigned)(-counter);
        }
        else
        {
            u = (unsigned)counter;
        }

        char rev[16];
        int ri = 0;
        while (u)
        {
            rev[ri++] = (char)('0' + (u % 10));
            u /= 10;
        }

        if (neg)
        {
            buf[pos++] = '-';
        }

        for (int j = ri - 1; j >= 0; --j)
        {
            buf[pos++] = rev[j];
        }
    }

    buf[pos] = '\0';
    return pos;
}

int format_unsigned_value(char *buf, const unsigned long value, const char* val)
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
        while (u)
        {
            rev[ri++] = (char)('0' + (u % 10));
            u /= 10;
        }

        for (int j = ri - 1; j >= 0; --j)
        {
            buf[pos++] = rev[j];
        }
    }

    buf[pos] = '\0';
    return pos;
}

int format_float_value(char *buf, double value, const char *val)
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
        buf[pos++] = '-';
        value = -value;
    }

    unsigned long whole = (unsigned long)value;
    double fraction = value - (double)whole;
    unsigned long scaled = (unsigned long)(fraction * 100.0 + 0.5);

    char rev[32];
    int ri = 0;
    do
    {
        rev[ri++] = (char)('0' + (whole % 10));
        whole /= 10;
    } while (whole != 0);

    for (int j = ri - 1; j >= 0; --j)
    {
        buf[pos++] = rev[j];
    }

    buf[pos++] = '.';
    buf[pos++] = (char)('0' + (scaled / 10));
    buf[pos++] = (char)('0' + (scaled % 10));
    buf[pos] = '\0';
    return pos;
}

uintptr_t read_stack_pointer(void)
{
    uintptr_t sp;

    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return sp;
}


uint64_t read_cycle_counter()
{
    uint64_t counter;

    /*
     * __volatile__ - tells the compiler not to remove or reorder the instruction.
     * "rdcycle" - reads the current cycle counter into the output operand.
     * %0 -  first output operand listed after the colon.
     * "=r"(seed) - stores the result in any general-purpose register
     *   & copies that value into the C variable `seed`.
     */
    __asm__ __volatile__("rdcycle %0" : "=r"(counter));
    counter ^= (uint64_t)(uintptr_t)&counter;
    return counter;
}

void computation()
{   
    struct rewind_state* state = state_anchor;
    unsigned long next = state->a + state->b;
    state->a = state->b;
    state->b = next;
    state->counter++;
};

int run_enclave(unsigned long runs, struct fault_model fault_model, int return_on_fault)
{
    struct rewind_state state = {0, 1, 0}; // fibonacci sequence init

    state_anchor = &state;

    // on restart, recover the last sealed checkpoint if the host has one
    if (load_checkpoint() == 0) 
    {
        if (restore_checkpoint() == 0) 
        {
            eapp_print("loading stack snapshot"); 
        }
    }
    
    eapp_print("Rewind enclave start");

    for (; state.counter < runs;)
    {
        char formated_counter[32], formated_fib[32];
        format_value(formated_counter, state.counter, "counter");
        format_unsigned_value(formated_fib, state.b, "output");
        
        eapp_print(formated_counter);
        eapp_print(formated_fib);

        // inject one modeled fault point using a simple pseudo-random splitex function
        // fault happens before so that the "computation" can fail
        if (fault_should_trigger(&fault_model)) 
        {
            eapp_print("Simulated fault");
            if (return_on_fault)
            {
                // benchmark mode can keep running when the fault is only recorded
                EAPP_RETURN(16);
            }

            break;
        }

        computation();
        

        if (save_checkpoint() != 0) 
        {
            eapp_print("failed to save stack checkpoint");
            //__builtin_trap();
            EAPP_RETURN(16);
        }       
    }

    EAPP_RETURN(0);
}
