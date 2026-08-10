#include "fault.h"

static uint64_t fault_splitmix64(uint64_t value)
{
    value += SPLITMIX64_GAMMA;
    value = (value ^ (value >> SPLITMIX64_SHIFT_1)) * SPLITMIX64_MUL1;
    value = (value ^ (value >> SPLITMIX64_SHIFT_2)) * SPLITMIX64_MUL2;
    return value ^ (value >> SPLITMIX64_SHIFT_3);
}

static void sort_unsigned_long_array(unsigned long* out, int k)
{
    for (int i = 0; i < k; i++)
    {
        int min_index = i;

        for (int j = i + 1; j < k; j++)
        {
            if (out[j] < out[min_index])
            {
                min_index = j;
            }
        }

        if (min_index != i)
        {
            unsigned long temp = out[i];
            out[i] = out[min_index];
            out[min_index] = temp;
        }
    }
}

void fill_range(unsigned long* out, int k, unsigned long N, int sorted)
{
    uint64_t seed = read_cycle_counter();
    if (out == NULL || k <= 0 || N == 0)
    {
        return;
    }

    for (int i = 0; i < k; i++)
    {
        seed += SPLITMIX64_GAMMA;
        out[i] = (unsigned long)(fault_splitmix64(seed) % (uint64_t)N);
    }

    if (sorted)
    {
        sort_unsigned_long_array(out, k);
    }
}

uint64_t fault_default_seed(void)
{
#if FAULT_RANDOMIZE_SEED
    return read_cycle_counter();
#else
    return SEED;
#endif
}

int find_fault_positions(struct fault_model* model,
                        unsigned long runs,
                        int K,
                        unsigned long* positions_out,
                        int saving)
{

    if (K < 0 || K > runs) {return -1;}

    if (K == 0) {return 0;}

    unsigned long window_start = model->step;
    unsigned long window_end = window_start + runs;
    int counter = 0;

    // initial run
    for (unsigned long step = window_start; step < window_end; step = increment_ulong_wrapped(step))
    {
        if (will_fault_trigger(model, (uint64_t)step))
        {   
            positions_out[counter] = step;
            counter++;
        }

        if (counter>=K) {break;}
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

        // keep the remaining faults relative to the new window start
        for (int i = 0; i < counter; i++)
        {
            positions_out[i]--;
        }

        window_start = increment_ulong_wrapped(window_start);
        window_end = increment_ulong_wrapped(window_end);

        // stop once the full sliding range has been exhausted
        if (window_start == search_origin) {return -1;}

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
    if (result == 0) {print_metric("fault", step);}
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
    model->step = increment_uint_wrapped(model->step);

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

uint64_t find_optimal_period(uint64_t fault_number, uint64_t run_number)
{
    if (fault_number == 0 || run_number == 0)
    {
        return 0;
    }

    uint64_t period = run_number / fault_number;
    
    print_metric("optimal runs", run_number);
    print_metric("optimal fault_number", fault_number);
    print_metric("optimal period", period);
    return period;
}
