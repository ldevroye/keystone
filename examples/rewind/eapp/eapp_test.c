#include "eapp_test.h"


static void print_cycle_metric(const char* label, uint64_t cycles)
{
    char buffer[96];
    format_unsigned_value(buffer, cycles, label);
    eapp_print(buffer);
}

static void print_indexed_cycle_metric(const char* prefix, int index, uint64_t cycles)
{
    char label[96];
    size_t prefix_len = 0;

    while (prefix[prefix_len] != '\0' && prefix_len + 3 < sizeof(label))
    {
        label[prefix_len] = prefix[prefix_len];
        prefix_len++;
    }

    if (prefix_len + 2 >= sizeof(label))
    {
        return;
    }

    label[prefix_len++] = '_';
    label[prefix_len++] = (char)('0' + index);
    label[prefix_len] = '\0';

    print_cycle_metric(label, cycles);
}

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

    int current_position = fault_positions[0];
    struct rewind_state state = {0, 1, 0};
    uint64_t count=0;

    while (state.counter < runs)
    {
        count++;
        if (state.counter = fault_positions[current_position])
        {
            if (!checkpoint_enabled)
            {
                state = (struct rewind_state) {0, 1, 0};
            }

            current_position++;
            continue;
        }
    }

    *total_iterations = count;
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
    if (save_checkpoint() != 0)
    {
        eapp_print("cycle breakdown save failed");
        return -1;
    }
    seal_end = read_cycle_counter();

    memset(&state, 0, sizeof(state));

    unseal_start = read_cycle_counter();
    if (load_checkpoint() != 0)
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

    print_cycle_metric("checkpoint_iv_bytes", iv_size);
    print_cycle_metric("checkpoint_plain_bytes", plain_checkpoint_size);
    print_cycle_metric("checkpoint_tag_bytes", (uint64_t)CHECKPOINT_TAG_SIZE);
    print_cycle_metric("checkpoint_sealed_bytes", sealed_blob_size);


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

    print_cycle_metric("checkpoint_seal_cycles", seal_cycles);
    print_cycle_metric("checkpoint_unseal_cycles", unseal_cycles);
    print_cycle_metric("checkpoint_compute_cycles", compute_cycles);
    return 0;
}

int run_break_even_test()
{
    enum
    {
        MAX_DETERMINISTIC_FAULTS = 3
    };

    uint64_t save_cycles, load_cycles, compute_cycles;
    measure_checkpoint_cycle_breakdown(&save_cycles, &load_cycles, &compute_cycles);

    // Use a fixed run budget so each K value is comparable.
    const unsigned long runs = 1000;
    unsigned long fault_positions_save[MAX_DETERMINISTIC_FAULTS];
    unsigned long fault_positions_no_save[MAX_DETERMINISTIC_FAULTS];
    uint64_t cost_no_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};
    uint64_t cost_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};
    uint64_t threshold_errors = MAX_DETERMINISTIC_FAULTS + 1;

    for (int k = 0; k <= MAX_DETERMINISTIC_FAULTS; k++)
    {
        struct fault_model fault_model = MODEL_DEFAULT;

        // Build a deterministic fault schedule with exactly k faults.
        uint64_t period_save = find_optimal_period(k, runs, 1);
        fault_model.period = period_save;   
        find_fault_positions(&fault_model, runs, k, fault_positions_save);

        // Baseline: execute the run without checkpointing.
        measure_scenario(runs, 0, fault_positions_no_save, k, &cost_no_save[k]);


        uint64_t period_no_save = find_optimal_period(k, runs, 0);
        fault_model.period = period_save;   
        find_fault_positions(&fault_model, runs, k, fault_positions_no_save);
        
        // Checkpointed path: restart from the last saved state after each fault.
        measure_scenario(runs, 1, fault_positions_save, k, &cost_save[k]);

        // Record both costs so the break-even point can be compared.
        print_indexed_cycle_metric("cost_no_save", k, cost_no_save[k]);
        print_indexed_cycle_metric("cost_save", k, cost_save[k]);
        // First K where checkpointing becomes cheaper than running through faults.
        threshold_errors = (uint64_t)k;
        
    }

    print_cycle_metric("break_even_threshold_errors", threshold_errors);

    return 0;
}

int run_eapp_tests()
{
    
#if EAPP_AVG_FAULT_TESTING
    eapp_print("[START] fault rate");
    avg_fault_test();
    eapp_print("[end] fault rate");

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
    // Break-even test workflow: generate a fault schedule, compare the
    // checkpointed and non-checkpointed runs, then report the threshold.
    eapp_print("[START] break even");

    if (run_break_even_test() != 0)
    {
        eapp_print("failed break-even test");
    }
    eapp_print("[END] break even");

#endif
    
    return 0;
}
