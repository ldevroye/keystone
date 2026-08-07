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


unsigned long advance_wrapped(unsigned long value)
{
    return value == ULONG_MAX ? 0UL : value + 1UL;
}

int generate_fault_positions(struct fault_model* model,
                                    unsigned long runs,
                                    int K,
                                    unsigned long* positions_out)
{

    if (K < 0 || K > runs) {return -1;}

    if (K == 0) {return 0;}

    unsigned long window_start = model->step;
    unsigned long window_end = window_start+runs;
    int counter = 0;

    // initial run
    for (unsigned long offset = 0; offset < runs; offset++)
    {
        if (will_fault_trigger(model, (uint64_t)offset))
        {
            positions_out[counter] = offset;
            counter++;
        }

        offset = advance_wrapped(offset);
    }

    if (counter >= K) {return 0;}


    // sliding window
    const unsigned long search_origin = window_start;
    while (counter < K)
    {   
        // remove first
        if (counter > 0 && positions_out[0] == window_start)
        {
            memmove(positions_out,
                    positions_out + 1,
                    (unsigned long)(counter - 1) * sizeof(unsigned long));
            counter--;
        }
        
        // if wrap around stop
        if (window_start == search_origin) {return -1;}

        window_start = advance_wrapped(window_start);
        window_end = advance_wrapped(window_end);

        // add last
        if (will_fault_trigger(model, (uint64_t)window_end))
        {
            positions_out[counter] = window_end;
            counter++;
        }
    }

    return 0;
}


int will_fault_trigger(struct fault_model *model, uint64_t step)
{   
    // mimics should_fault_trigger withot incrementing the step
    // used to find an error pattern
    if (model == NULL)
    {
        eapp_print("Error: fault model is null");
        return 0;
    }

    if (model->period == 0) 
    {
        eapp_print("Error: fault period is zero");
        return 0;
    }


    uint64_t current_step = model->seed + step;
    uint64_t error = fault_splitmix64(current_step);
    uint64_t result = error % model->period;
    return result == 0;
}


int should_fault_trigger(struct fault_model* model)
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

int compute_fault_rate(uint64_t fault_number, uint64_t run_number, int saving)
{
    if (fault_number == 0 || run_number == 0)
    {
        return 0;
    }

    uint64_t total_runs = run_number;

    if (saving)
    {
        total_runs *= (fault_number + 1);
    }

    uint64_t period = total_runs / fault_number;
    if (total_runs % fault_number != 0)
    {
        period++;
    }

    if (period > (uint64_t)INT_MAX)
    {
        return INT_MAX;
    }

    return (int)period;
}
