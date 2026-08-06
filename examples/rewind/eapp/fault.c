#include "fault.h"

static uint64_t fault_splitmix64(uint64_t value)
{
    value += SPLITMIX64_GAMMA;
    value = (value ^ (value >> SPLITMIX64_SHIFT_1)) * SPLITMIX64_MUL1;
    value = (value ^ (value >> SPLITMIX64_SHIFT_2)) * SPLITMIX64_MUL2;
    return value ^ (value >> SPLITMIX64_SHIFT_3);
}

uint64_t fault_default_seed(void)
{
#if FAULT_RANDOMIZE_SEED
    return read_cycle_counter();
#else
    return SEED;
#endif
}

int fault_should_trigger(struct fault_model* model)
{
    if (model == 0) 
    {
        eapp_print("Error: fault model is null");
        return 0;
    }

    if (model->period == 0) 
    {
        eapp_print("Error: fault period is zero");
        return 0;
    }

    
    // simple bound to make sure across long runs
    if (model->step == UINT64_MAX) 
    { 
        model->step = 0;
    }
    model->step++;
    

    uint64_t current_step = model->seed + model->step;
    uint64_t error = fault_splitmix64(current_step);
    uint64_t result = error % model->period; // 1/period chance of error 

    return result == 0;
}

struct fault_model get_default_model()
{   
    // runtime seed allocator
    struct fault_model ret = {fault_default_seed(), 0, PERIOD};
    return ret;
}
