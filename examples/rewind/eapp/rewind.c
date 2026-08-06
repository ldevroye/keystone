#include <string.h>

#include "app/syscall.h"
#include "app/eapp_utils.h"

#include "common.h"
#include "checkpoint.h"
#include "eapp_test.h"
#include "fault.h"

int main() 
{
#if EAPP_TESTING
    eapp_print("testing mode enabled");
    EAPP_RETURN(run_eapp_tests());
#else
    struct fault_model fault_model=get_default_model();
    EAPP_RETURN(run_enclave(EAPP_RUNS, &fault_model));
#endif
} 