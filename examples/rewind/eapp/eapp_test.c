#include "eapp_test.h"


static int measure_scenario(unsigned long runs,
                            int checkpoint_enabled,
                            const unsigned long* fault_positions,
                            size_t len_fault_positions,
                            uint64_t* total_iterations)
{   
    if (len_fault_positions == 0)
    {
        *total_iterations=runs;
        return 0;
    }

    if (checkpoint_enabled) 
    {
        *total_iterations=runs+len_fault_positions;
        return 0;
    }
    
    uint64_t total_count=0;
    for (auto i; i<len_fault_positions; i++)
    {
        total_count += fault_positions[i];
    }

    total_count += runs;

    *total_iterations = total_count;
    return 0;
}

static int measure_checkpoint_cycle_breakdown(uint64_t* seal_cycles, uint64_t* unseal_cycles, uint64_t* compute_cycles)
{
    struct rewind_state state = {1, 2, 3};
    uint64_t seal_start;
    uint64_t seal_end;
    uint64_t unseal_start;
    uint64_t unseal_end;
    uint64_t compute_start;
    uint64_t compute_end;

    state_anchor = &state;

    seal_start = read_cycle_counter();
    if (save_checkpoint(0) != 0)
    {
        eapp_print("cycle breakdown save failed");
        return -1;
    }
    seal_end = read_cycle_counter();

    memset(&state, 0, sizeof(state));

    unseal_start = read_cycle_counter();
    if (load_checkpoint(0) != 0)
    {
        eapp_print("cycle breakdown load failed");
        return -1;
    }
    if (restore_checkpoint() != 0)
    {
        eapp_print("cycle breakdown restore failed");
        return -1;
    }
    unseal_end = read_cycle_counter();

    if (state.a != 1 || state.b != 2 || state.counter != 3)
    {
        eapp_print("cycle breakdown validation failed");
        return -1;
    }

    compute_start = read_cycle_counter();
    computation();
    compute_end = read_cycle_counter();
    

    *seal_cycles = seal_end - seal_start;
    *unseal_cycles = unseal_end - unseal_start;
    *compute_cycles = compute_end - compute_start;
    return 0;
}

int run_blob_size_test()
{
    struct checkpoint checkpoint;
    struct sealed_checkpoint blob;
    uint64_t plain_checkpoint_size;
    uint64_t sealed_blob_size;
    uint64_t iv_size;

    memset(&checkpoint, 0, sizeof(checkpoint));
    memset(&blob, 0, sizeof(blob));

    checkpoint.checkpoint_seq = 7;
    state_anchor = (struct rewind_state *)&checkpoint.stack_data[STACK_SNAPSHOT_SIZE - sizeof(struct rewind_state)];
    memcpy(state_anchor, &(struct rewind_state){1, 2, 3}, sizeof(struct rewind_state));

    if (seal_checkpoint_blob(&blob, &checkpoint) != 0)
    {
        eapp_print("blob size test sealing failed");
        return -1;
    }

    plain_checkpoint_size = (uint64_t)sizeof(struct checkpoint);
    sealed_blob_size = (uint64_t)sizeof(struct sealed_checkpoint);
    iv_size = AES_BLOCK_SIZE;

    print_metric("checkpoint_iv_bytes ", iv_size);
    print_metric("checkpoint_plain_bytes ", plain_checkpoint_size);
    print_metric("checkpoint_tag_bytes ", (uint64_t)CHECKPOINT_TAG_SIZE);
    print_metric("checkpoint_sealed_bytes ", sealed_blob_size);


    return 0;
}

void avg_fault_test()
{
    struct fault_model fault_model = MODEL_DEFAULT;
    int counter = 0;
    int nb_faults = 0;

    for (; counter < 10000; counter++)
    {
        if (should_fault_trigger(&fault_model))
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
    eapp_print(formated_rate);
}

int run_round_trip_test()
{
    struct rewind_state expected_state = {1, 2, 3};
    struct rewind_state restored_state;
    struct checkpoint checkpoint;
    struct sealed_checkpoint blob;
    const size_t state_offset = STACK_SNAPSHOT_SIZE - sizeof(expected_state);

    memset(&checkpoint, 0, sizeof(checkpoint));
    memset(&restored_state, 0, sizeof(restored_state));
    memset(&blob, 0, sizeof(blob));

    checkpoint.checkpoint_seq = 42;
    memset(checkpoint.stack_data, 0xA5, sizeof(checkpoint.stack_data));
    memcpy(checkpoint.stack_data + state_offset, &expected_state, sizeof(expected_state));

    if (seal_checkpoint_blob(&blob, &checkpoint) != 0)
    {
        eapp_print("round-trip test sealing failed");
        return -1;
    }

    if (open_checkpoint_blob(&checkpoint, &blob) != 0)
    {
        eapp_print("round-trip test opening failed");
        return -1;
    }

    memcpy(&restored_state,
           checkpoint.stack_data + (STACK_SNAPSHOT_SIZE - sizeof(restored_state)),
           sizeof(restored_state));

    if (restored_state.a != expected_state.a ||
        restored_state.b != expected_state.b ||
        restored_state.counter != expected_state.counter)
    {
        eapp_print("round-trip test validation failed");
        return -1;
    }

    eapp_print("round-trip test passed");
    return 0;
}

int run_cycle_breakdown_test()
{
    uint64_t seal_cycles = 0;
    uint64_t unseal_cycles = 0;
    uint64_t compute_cycles = 0;

    if (measure_checkpoint_cycle_breakdown(&seal_cycles, &unseal_cycles, &compute_cycles) != 0)
    {
        return -1;
    }

    print_metric("checkpoint_seal_cycles ", seal_cycles);
    print_metric("checkpoint_unseal_cycles ", unseal_cycles);
    print_metric("checkpoint_compute_cycles ", compute_cycles);
    return 0;
}

int run_break_even_test()
{
    enum
    {
        MAX_DETERMINISTIC_FAULTS = 10
    };

    uint64_t save_cycles, load_cycles, compute_cycles;
    if (measure_checkpoint_cycle_breakdown(&save_cycles, &load_cycles, &compute_cycles) != 0)
        return -1;

    const unsigned long runs_values[] = {1000UL, 10000UL, 100000UL, 1000000UL, 10000000UL, 100000000UL};
    const int runs_count = sizeof(runs_values) / sizeof(runs_values[0]);
    const unsigned long avg_runs = 1000; // lowered for practicality across many run values

    unsigned long fault_positions_save[MAX_DETERMINISTIC_FAULTS];
    unsigned long fault_positions_no_save[MAX_DETERMINISTIC_FAULTS];

    const uint64_t CPU_FREQ_HZ = 4370000000ULL; // my computa 
    const uint64_t measured_compute = compute_cycles*100000ULL;
    const uint64_t save_cost_per_iter = (save_cycles + load_cycles + measured_compute);
    const uint64_t no_save_cost_per_iter = measured_compute;

    for (int rv = 0; rv < runs_count; rv++)
    {
        unsigned long runs = runs_values[rv];
        print_indexed_metric("-------- break_even_runs --------", rv, (uint64_t)runs);

        uint64_t cost_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};
        uint64_t cost_no_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};

        for (int k = 0; k < MAX_DETERMINISTIC_FAULTS+1; k++)
        {
            uint64_t current_save = 0;
            uint64_t current_no_save = 0;

            for (unsigned long i = 0; i < avg_runs; i++)
            {
                fill_range(fault_positions_save, k, runs, 1);
                measure_scenario(runs, 1, fault_positions_save, k, &current_save);

                fill_range(fault_positions_no_save, k, runs, 0);
                measure_scenario(runs, 0, fault_positions_no_save, k, &current_no_save);

                cost_save[k] += current_save;
                cost_no_save[k] += current_no_save;
            }

            cost_save[k] /= avg_runs;
            cost_no_save[k] /= avg_runs;

            const uint64_t nbr_iter_save = cost_save[k];
            const uint64_t nbr_iter_no_save = cost_no_save[k];
            const uint64_t total_saving_cycles = nbr_iter_save * save_cost_per_iter;
            const uint64_t total_no_save_cycles = nbr_iter_no_save * no_save_cost_per_iter;

            print_indexed_metric("cost save_cycle   ", nbr_iter_save, save_cost_per_iter);
            print_indexed_metric("cost no_save_cycle ", nbr_iter_no_save, no_save_cost_per_iter);
            const uint64_t ratio = total_no_save_cycles/total_saving_cycles;

            char buffer[96] = "";

            if (total_no_save_cycles >= total_saving_cycles)
            {
                const char* prefix="POSITIVE cycle ratio ";
                memcpy(buffer, prefix, strlen(prefix));
                print_indexed_metric(buffer, k, ratio);
            }
            else
            {
                const char* char_negative = "NEGATIVE cycle ratio ";
                memcpy(buffer, char_negative, strlen(char_negative));
                print_metric(buffer, ratio);
            }

            /*
            // also print deltas in microseconds
            uint64_t total_saving_time_us = (total_saving_cycles * 1000000ULL + (CPU_FREQ_HZ / 2)) / CPU_FREQ_HZ;
            uint64_t total_no_save_time_us = (total_no_save_cycles * 1000000ULL + (CPU_FREQ_HZ / 2)) / CPU_FREQ_HZ;
            uint64_t time_ratio = total_no_save_time_us/total_saving_time_us;
            */
        }
    }

    return 0;
}

int run_eapp_tests()
{

#if EAPP_AVG_FAULT_TESTING
    eapp_print("[START] fault rate");
    avg_fault_test();
    eapp_print("[END] fault rate");
#endif

#if EAPP_ROUND_TRIP_TESTING
    eapp_print("[START] round trip");
    if (run_round_trip_test() != 0)
    {
        eapp_print("failed run round-trip");
    }
    eapp_print("[END] round trip");
#endif

#if EAPP_BLOB_SIZE_TESTING
    eapp_print("[START] blob size");
    if (run_blob_size_test() != 0)
    {
        eapp_print("failed blob size test");
    }
    eapp_print("[END] blob size");
#endif

#if EAPP_CYCLE_BREAKDOWN_TESTING
    eapp_print("[START] cycle breakdown");
    if (run_cycle_breakdown_test() != 0)
    {
        eapp_print("failed cycle breakdown test");
    }
    eapp_print("[END] cycle breakdown");
#endif

#if EAPP_BREAK_EVEN_TESTING
    eapp_print("[START] break even");
    if (run_break_even_test() != 0)
    {
        eapp_print("failed break-even test");
    }
    eapp_print("[END] break even");
#endif

    return 0;
}
