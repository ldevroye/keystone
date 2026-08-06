#include <string.h>

#include "common.h"
#include "app/syscall.h"
#include "app/eapp_utils.h"
#include "checkpoint.h"
#include "eapp_test.h"
#include "fault.h"

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


int main() 
{

#if EAPP_TESTING
    eapp_print("testing mode enabled");
    EAPP_RETURN(run_eapp_tests());
#else
    run_enclave(EAPP_RUNS, get_default_model(), 1);
#endif
} 