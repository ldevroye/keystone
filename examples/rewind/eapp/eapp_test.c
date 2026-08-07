#include <stddef.h>
#include <limits.h>
#include <string.h>

#include "common.h"
#include "checkpoint.h"
#include "eapp_test.h"
#include "fault.h"

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
                            int K,
                            uint64_t* elapsed_cycles)
{
    struct rewind_state state = {0, 1, 0};
    unsigned long fault_index = 0;
    uint64_t start_cycles;
    uint64_t end_cycles;

    state_anchor = &state;
    start_cycles = read_cycle_counter();

    while (state.counter < runs)
    {
        if (fault_index < (unsigned long)K && state.counter == fault_positions[fault_index])
        {
            if (checkpoint_enabled)
            {
                if (load_checkpoint() != 0)
                {
                    eapp_print("deterministic break-even load failed");
                    return -1;
                }

                if (restore_checkpoint() != 0)
                {
                    eapp_print("deterministic break-even restore failed");
                    return -1;
                }
            }
            else
            {
                state.a = 0;
                state.b = 1;
                state.counter = 0;
            }

            fault_index++;
            continue;
        }

        computation();

        if (checkpoint_enabled)
        {
            if (save_checkpoint() != 0)
            {
                eapp_print("deterministic break-even save failed");
                return -1;
            }
        }
    }

    end_cycles = read_cycle_counter();
    *elapsed_cycles = end_cycles - start_cycles;
    return 0;
}

static int measure_checkpoint_cycle_breakdown(uint64_t* save_cycles, uint64_t* load_cycles)
{
    struct rewind_state state = {1, 2, 3};
    uint64_t save_start;
    uint64_t save_end;
    uint64_t load_start;
    uint64_t load_end;

    state_anchor = &state;

    save_start = read_cycle_counter();
    if (save_checkpoint() != 0)
    {
        eapp_print("cycle breakdown save failed");
        return -1;
    }
    save_end = read_cycle_counter();

    memset(&state, 0, sizeof(state));

    load_start = read_cycle_counter();
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
    load_end = read_cycle_counter();

    if (state.a != 1 || state.b != 2 || state.counter != 3)
    {
        eapp_print("cycle breakdown validation failed");
        return -1;
    }

    *save_cycles = save_end - save_start;
    *load_cycles = load_end - load_start;
    return 0;
}

int run_blob_size_test()
{
    struct checkpoint checkpoint;
    struct sealed_checkpoint blob;
    uint64_t plain_checkpoint_size;
    uint64_t sealed_blob_size;

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

    print_cycle_metric("checkpoint_plain_bytes", plain_checkpoint_size);
    print_cycle_metric("checkpoint_sealed_bytes", sealed_blob_size);
    print_cycle_metric("checkpoint_tag_bytes", (uint64_t)CHECKPOINT_TAG_SIZE);

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
    uint64_t save_cycles = 0;
    uint64_t load_cycles = 0;

    if (measure_checkpoint_cycle_breakdown(&save_cycles, &load_cycles) != 0)
    {
        return -1;
    }

    print_cycle_metric("checkpoint_save_cycles", save_cycles);
    print_cycle_metric("checkpoint_load_cycles", load_cycles);
    return 0;
}

int run_break_even_test()
{
    enum
    {
        MAX_DETERMINISTIC_FAULTS = 3
    };

    const unsigned long deterministic_runs = EAPP_RUNS * 4;
    unsigned long fault_positions[MAX_DETERMINISTIC_FAULTS];
    uint64_t cost_no_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};
    uint64_t cost_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};
    uint64_t threshold_errors = MAX_DETERMINISTIC_FAULTS + 1;

    for (int k = 0; k <= MAX_DETERMINISTIC_FAULTS; k++)
    {
        struct fault_model fault_model = MODEL_DEFAULT;

        auto ret = generate_fault_positions(&fault_model, deterministic_runs, k, fault_positions);
        while (ret != 0)
        {   
            ret = generate_fault_positions(&fault_model, deterministic_runs, k, fault_positions);
        }

        if (measure_scenario(deterministic_runs, 0, fault_positions, k, &cost_no_save[k]) != 0)
        {
            eapp_print("deterministic break-even no-save scenario failed");
            return -1;
        }

        if (measure_scenario(deterministic_runs, 1, fault_positions, k, &cost_save[k]) != 0)
        {
            eapp_print("deterministic break-even save scenario failed");
            return -1;
        }

        print_indexed_cycle_metric("cost_no_save", k, cost_no_save[k]);
        print_indexed_cycle_metric("cost_save", k, cost_save[k]);

        if (threshold_errors == MAX_DETERMINISTIC_FAULTS + 1 && cost_save[k] < cost_no_save[k])
        {
            threshold_errors = (uint64_t)k;
        }
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
    

#if EAPP_BREAK_EVEN_TESTING
    eapp_print("[START] break even");

    if (run_break_even_test() != 0)
    {
        eapp_print("failed break-even test");
    }
    eapp_print("[END] break even");

#endif

#if EAPP_CYCLE_BREAKDOWN_TESTING
    eapp_print("[START] cycle breakdown");

    if (run_cycle_breakdown_test() != 0)
    {
        eapp_print("failed cycle breakdown test");
    }

    eapp_print("[END] cycle breakdown");

#endif
    
    return 0;
}
