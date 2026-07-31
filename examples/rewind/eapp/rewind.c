#include <string.h>

#include "app/syscall.h"
#include "app/eapp_utils.h"
#include "checkpoint.h"
#include "eapp_test.h"
#include "fault.h"

#define OCALL_PRINT_BUFFER 1

#ifndef EAPP_RUNS
#define EAPP_RUNS 50
#endif

#ifndef EAPP_LOGGING
#define EAPP_LOGGING 1
#endif

#ifndef ENABLE_TESTING
#define ENABLE_TESTING 0
#endif

#ifndef ANALYSIS_RUNS
#define ANALYSIS_RUNS 20
#endif

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


int main() 
{

#if ENABLE_TESTING && ANALYSIS_RUNS<2
    eapp_print("testing mode enabled");
    EAPP_RETURN(run_eapp_tests);
#endif

    struct rewind_state state = {0, 1, 0}; // fibonacci sequence init
    struct rewind_checkpoint checkpoint; // empty for now as we will try to load the stack into it
    struct fault_model fault_model = get_default_model();

    // on restart, recover the last sealed checkpoint if the host has one
    if (load_checkpoint(&checkpoint) == 0) 
    {
        if (restore_checkpoint(&state, &checkpoint) == 0) 
        {
            eapp_print("loading stack snapshot"); 
        }
    }
    
    eapp_print("Rewind enclave start");

    for (; state.counter < EAPP_RUNS;)
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
            //asm volatile("unimp"); // returns illegal RISC-V instruction
            //__builtin_trap();
            EAPP_RETURN(16);
            //return 16; // Keystone::Error::EnclaveInterrupted
        }

        unsigned long next = state.a + state.b;
        state.a = state.b;
        state.b = next;
        state.counter++;

        if (save_checkpoint((uintptr_t)&state, sizeof(state)) != 0) 
        {
            eapp_print("failed to save stack checkpoint");
            //__builtin_trap();
            EAPP_RETURN(16);
        }       
    }

    EAPP_RETURN(0);
} 