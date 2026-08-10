#include "eapp_test.h"

#include <stdio.h>

static size_t append_literal(char* buffer, size_t size, size_t pos, const char* literal)
{
    const size_t literal_len = strlen(literal);
    const size_t remaining = size > pos ? size - pos : 0;
    const size_t copy_len = literal_len < remaining ? literal_len : remaining > 0 ? remaining - 1 : 0;
    if (copy_len > 0)
    {
        memcpy(buffer + pos, literal, copy_len);
        pos += copy_len;
    }
    if (pos < size)
    {
        buffer[pos] = '\0';
    }
    return pos;
}

static size_t append_unsigned(char* buffer, size_t size, size_t pos, unsigned long value)
{
    char digits[32];
    int length = 0;

    do
    {
        digits[length++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    while (length > 0 && pos < size)
    {
        buffer[pos++] = digits[--length];
    }

    if (pos < size)
    {
        buffer[pos] = '\0';
    }
    return pos;
}

static size_t append_double_two_dec(char* buffer, size_t size, size_t pos, double value)
{
    const unsigned long whole = (unsigned long)value;
    const double fraction = value - (double)whole;
    const unsigned long scaled = (unsigned long)(fraction * 100.0 + 0.5);

    pos = append_unsigned(buffer, size, pos, whole);
    if (pos < size)
    {
        buffer[pos++] = '.';
    }
    if (pos < size)
    {
        buffer[pos++] = (char)('0' + (scaled / 10));
    }
    if (pos < size)
    {
        buffer[pos++] = (char)('0' + (scaled % 10));
    }
    if (pos < size)
    {
        buffer[pos] = '\0';
    }
    return pos;
}

static void emit_break_even_csv(unsigned long runs, int k, unsigned long measured_compute, double ratio)
{
    char csv_line[256];
    size_t pos = 0;

    pos = append_literal(csv_line, sizeof(csv_line), pos, "break_even_threshold,");
    pos = append_unsigned(csv_line, sizeof(csv_line), pos, runs);
    pos = append_literal(csv_line, sizeof(csv_line), pos, ",");
    pos = append_unsigned(csv_line, sizeof(csv_line), pos, (unsigned long)k);
    pos = append_literal(csv_line, sizeof(csv_line), pos, ",");
    pos = append_unsigned(csv_line, sizeof(csv_line), pos, measured_compute);
    pos = append_literal(csv_line, sizeof(csv_line), pos, ",");
    append_double_two_dec(csv_line, sizeof(csv_line), pos, ratio);
    eapp_print(csv_line);
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

static int measure_checkpoint_cycle_breakdown(uint64_t* derive_cycles,
                                             uint64_t* seal_cycles,
                                             uint64_t* unseal_cycles,
                                             uint64_t* compute_cycles,
                                             struct checkpoint_crypto_metrics* crypto_metrics_out)
{
    struct rewind_state expected_state = {1, 2, 3};
    struct rewind_state restored_state;
    struct checkpoint checkpoint;
    struct sealed_checkpoint blob;
    const size_t state_offset = STACK_SNAPSHOT_SIZE - sizeof(expected_state);
    struct checkpoint_crypto_metrics crypto_metrics;

    memset(&checkpoint, 0, sizeof(checkpoint));
    memset(&restored_state, 0, sizeof(restored_state));
    memset(&blob, 0, sizeof(blob));
    memset(&crypto_metrics, 0, sizeof(crypto_metrics));
    state_anchor = &restored_state;

    checkpoint.checkpoint_seq = 42;
    memset(checkpoint.stack_data, 0xA5, sizeof(checkpoint.stack_data));
    memcpy(checkpoint.stack_data + state_offset, &expected_state, sizeof(expected_state));

    uint64_t derive_start = read_cycle_counter();
    if (derive_checkpoint_material() != 0)
    {
        eapp_print("cycle breakdown derive failed");
        return -1;
    }
    uint64_t derive_end = read_cycle_counter();

    uint64_t seal_start = read_cycle_counter();
    if (seal_checkpoint_blob_profile(&blob, &checkpoint, &crypto_metrics) != 0)
    {
        eapp_print("cycle breakdown seal failed");
        return -1;
    }
    uint64_t seal_end = read_cycle_counter();

    uint64_t unseal_start = read_cycle_counter();
    if (unseal_checkpoint_blob_profile(&checkpoint, &blob, &crypto_metrics) != 0)
    {
        eapp_print("cycle breakdown unseal failed");
        return -1;
    }
    uint64_t unseal_end = read_cycle_counter();

    memcpy(&restored_state,
           checkpoint.stack_data + state_offset,
           sizeof(restored_state));

    if (restored_state.a != expected_state.a ||
        restored_state.b != expected_state.b ||
        restored_state.counter != expected_state.counter)
    {
        eapp_print("cycle breakdown validation failed");
        return -1;
    }

    uint64_t compute_start = read_cycle_counter();
    computation();
    uint64_t compute_end = read_cycle_counter();

    *derive_cycles = derive_end - derive_start;
    *seal_cycles = seal_end - seal_start;
    *unseal_cycles = unseal_end - unseal_start;
    *compute_cycles = compute_end - compute_start;

    if (crypto_metrics_out != NULL)
    {
        *crypto_metrics_out = crypto_metrics;
    }
    return 0;
}

static int measure_checkpoint_cycle_breakdown_avg(uint64_t* derive_cycles,
                                                    uint64_t* seal_cycles,
                                                    uint64_t* unseal_cycles,
                                                    uint64_t* compute_cycles,
                                                    struct checkpoint_crypto_metrics* crypto_metrics_out)
{
    const unsigned long avg_runs = 1000UL;
    uint64_t derive_sum = 0;
    uint64_t seal_sum = 0;
    uint64_t unseal_sum = 0;
    uint64_t compute_sum = 0;
    struct checkpoint_crypto_metrics crypto_sum;

    memset(&crypto_sum, 0, sizeof(crypto_sum));

    for (unsigned long run = 0; run < avg_runs; run++)
    {
        uint64_t derive_once = 0;
        uint64_t seal_once = 0;
        uint64_t unseal_once = 0;
        uint64_t compute_once = 0;
        struct checkpoint_crypto_metrics crypto_once;

        memset(&crypto_once, 0, sizeof(crypto_once));

        if (measure_checkpoint_cycle_breakdown(&derive_once,
                                               &seal_once,
                                               &unseal_once,
                                               &compute_once,
                                               &crypto_once) != 0)
        {
            return -1;
        }

        derive_sum += derive_once;
        seal_sum += seal_once;
        unseal_sum += unseal_once;
        compute_sum += compute_once;

        crypto_sum.seal_prep_cycles += crypto_once.seal_prep_cycles;
        crypto_sum.seal_encrypt_cycles += crypto_once.seal_encrypt_cycles;
        crypto_sum.seal_tag_cycles += crypto_once.seal_tag_cycles;
        crypto_sum.seal_copy_cycles += crypto_once.seal_copy_cycles;
        crypto_sum.unseal_copy_in_cycles += crypto_once.unseal_copy_in_cycles;
        crypto_sum.unseal_tag_cycles += crypto_once.unseal_tag_cycles;
        crypto_sum.unseal_compare_cycles += crypto_once.unseal_compare_cycles;
        crypto_sum.unseal_decrypt_cycles += crypto_once.unseal_decrypt_cycles;
        crypto_sum.unseal_copy_out_cycles += crypto_once.unseal_copy_out_cycles;
    }

    *derive_cycles = derive_sum / avg_runs;
    *seal_cycles = seal_sum / avg_runs;
    *unseal_cycles = unseal_sum / avg_runs;
    *compute_cycles = compute_sum / avg_runs;

    if (crypto_metrics_out != NULL)
    {
        crypto_metrics_out->seal_prep_cycles = crypto_sum.seal_prep_cycles / avg_runs;
        crypto_metrics_out->seal_encrypt_cycles = crypto_sum.seal_encrypt_cycles / avg_runs;
        crypto_metrics_out->seal_tag_cycles = crypto_sum.seal_tag_cycles / avg_runs;
        crypto_metrics_out->seal_copy_cycles = crypto_sum.seal_copy_cycles / avg_runs;
        crypto_metrics_out->unseal_copy_in_cycles = crypto_sum.unseal_copy_in_cycles / avg_runs;
        crypto_metrics_out->unseal_tag_cycles = crypto_sum.unseal_tag_cycles / avg_runs;
        crypto_metrics_out->unseal_compare_cycles = crypto_sum.unseal_compare_cycles / avg_runs;
        crypto_metrics_out->unseal_decrypt_cycles = crypto_sum.unseal_decrypt_cycles / avg_runs;
        crypto_metrics_out->unseal_copy_out_cycles = crypto_sum.unseal_copy_out_cycles / avg_runs;
    }

    return 0;
}

static void print_cycle_sum(const char* total_label,
                                   uint64_t total_cycles,
                                   uint64_t part_cycles[],
                                   size_t len_part_cycles)
{   

    uint64_t part_sum=0;
    for (int i =0; i<len_part_cycles; i++)
    {
        part_sum += part_cycles[i];
    }
    
    print_metric(total_label, total_cycles);
    print_metric("parts_sum ", part_sum);

    if (part_sum >= total_cycles)
    {
        print_metric("diff: ", part_sum - total_cycles);
    }
    else
    {
        print_metric("diff: ", total_cycles- part_sum);

    }
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

    if (unseal_checkpoint_blob(&checkpoint, &blob) != 0)
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
    uint64_t derive_cycles = 0;
    uint64_t seal_cycles = 0;
    uint64_t unseal_cycles = 0;
    uint64_t compute_cycles = 0;
    struct checkpoint_crypto_metrics crypto_metrics;

    if (measure_checkpoint_cycle_breakdown_avg(&derive_cycles,
                                                &seal_cycles,
                                                &unseal_cycles,
                                                &compute_cycles,
                                                &crypto_metrics) != 0)
    {
        return -1;
    }

    print_metric("checkpoint_derive_cycles ", derive_cycles);
    print_metric("checkpoint_seal_prep_cycles ", crypto_metrics.seal_prep_cycles);
    print_metric("checkpoint_seal_encrypt_cycles ", crypto_metrics.seal_encrypt_cycles);
    print_metric("checkpoint_seal_tag_cycles ", crypto_metrics.seal_tag_cycles);
    print_metric("checkpoint_seal_copy_cycles ", crypto_metrics.seal_copy_cycles);
    uint64_t seal_parts[] = {
        crypto_metrics.seal_prep_cycles,
        crypto_metrics.seal_encrypt_cycles,
        crypto_metrics.seal_tag_cycles,
        crypto_metrics.seal_copy_cycles,
    };
    print_metric("checkpoint_unseal_copy_in_cycles ", crypto_metrics.unseal_copy_in_cycles);
    print_metric("checkpoint_unseal_tag_cycles ", crypto_metrics.unseal_tag_cycles);
    print_metric("checkpoint_unseal_compare_cycles ", crypto_metrics.unseal_compare_cycles);
    print_metric("checkpoint_unseal_decrypt_cycles ", crypto_metrics.unseal_decrypt_cycles);
    print_metric("checkpoint_unseal_copy_out_cycles ", crypto_metrics.unseal_copy_out_cycles);
    uint64_t unseal_parts[] = {
        crypto_metrics.unseal_copy_in_cycles,
        crypto_metrics.unseal_tag_cycles,
        crypto_metrics.unseal_compare_cycles,
        crypto_metrics.unseal_decrypt_cycles,
        crypto_metrics.unseal_copy_out_cycles,
    };

    print_cycle_sum("checkpoint_seal_cycles ", seal_cycles, seal_parts, sizeof(seal_parts) / sizeof(seal_parts[0]));
    print_cycle_sum("checkpoint_unseal_cycles ", unseal_cycles, unseal_parts, sizeof(unseal_parts) / sizeof(unseal_parts[0]));
    print_metric("checkpoint_compute_cycles ", compute_cycles);
    return 0;
}

int run_break_even_test()
{
    enum
    {
        MAX_DETERMINISTIC_FAULTS = 9
    };

    uint64_t save_cycles, load_cycles, compute_cycles;
    
    // rt test
    //if (measure_checkpoint_cycle_breakdown(&save_cycles, &load_cycles, &compute_cycles) != 0)
        //return -1;

    const unsigned long thousand=1000UL;
    const unsigned long hundred_thousand=100*thousand;
    const unsigned long million=thousand*thousand;
    const unsigned long ten_million=10*million;

    save_cycles = 50*million;
    load_cycles = save_cycles;
    compute_cycles=50*thousand;


    const unsigned long runs_values[] = {10*thousand, hundred_thousand, million, ten_million, 5*ten_million};
    const unsigned long compute_cost_values[] = {compute_cycles, hundred_thousand, 2*hundred_thousand, 5*hundred_thousand, 8*hundred_thousand, 
                                                million, 2*million, 5*million, 8*million,
                                                ten_million, 2*ten_million, 3*ten_million, 4*ten_million, 5*ten_million,
                                                6*ten_million, 7*ten_million, 8*ten_million, 9*ten_million, 10*ten_million,
                                                11*ten_million, 12*ten_million, 13*ten_million, 14*ten_million, 15*ten_million,
                                                16*ten_million, 17*ten_million, 18*ten_million, 19*ten_million, 20*ten_million
                                                
                                            };
    const unsigned long avg_runs = 1000; // lowered for practicality across many run values
    unsigned long fault_positions_save[MAX_DETERMINISTIC_FAULTS];
    unsigned long fault_positions_no_save[MAX_DETERMINISTIC_FAULTS];

    const uint64_t CPU_FREQ_HZ = 4370000000ULL; // my computa 
    

    const int runs_count = sizeof(runs_values) / sizeof(runs_values[0]);
    const int compute_count = sizeof(compute_cost_values) / sizeof(compute_cost_values[0]);

    for (int rv = 0; rv < runs_count; rv++)
    {
        const unsigned long runs = runs_values[rv];
        print_indexed_metric("-------- break_even_runs -------- ", rv, (uint64_t)runs);

        uint64_t cost_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};
        uint64_t cost_no_save[MAX_DETERMINISTIC_FAULTS + 1] = {0};

        for (int k = 0; k < MAX_DETERMINISTIC_FAULTS+1; k++)
        {
            uint64_t current_save = 0;
            uint64_t current_no_save = 0;
            uint64_t min_no_save_error_sum = UINT64_MAX;
            uint64_t max_no_save_error_sum = 0;

            for (unsigned long i = 0; i < avg_runs; i++)
            {
                fill_range(fault_positions_save, k, runs, 1);
                measure_scenario(runs, 1, fault_positions_save, k, &current_save);

                fill_range(fault_positions_no_save, k, runs, 0);
                measure_scenario(runs, 0, fault_positions_no_save, k, &current_no_save);

                if (current_no_save < min_no_save_error_sum)
                {
                    min_no_save_error_sum = current_no_save;
                }
                if (current_no_save > max_no_save_error_sum)
                {
                    max_no_save_error_sum = current_no_save;
                }

                cost_save[k] += current_save;
                cost_no_save[k] += current_no_save;
            }

            cost_save[k] /= avg_runs;
            cost_no_save[k] /= avg_runs;

#if EAPP_BREAK_EVEN_CSV_OUTPUT
            print_indexed_metric("break_even no_save min_error_sum ", k, min_no_save_error_sum);
            print_indexed_metric("break_even no_save max_error_sum ", k, max_no_save_error_sum);
#endif
            int threshold_reached = 0;
            for (int cp = 0; cp < compute_count; cp++)
            {   
                threshold_reached = 0;
                const unsigned long measured_compute = compute_cost_values[cp];
                const uint64_t save_cost_per_iter = (save_cycles + measured_compute);
                const uint64_t no_save_cost_per_iter = measured_compute;
                const uint64_t nbr_iter_save = cost_save[k];
                const uint64_t nbr_iter_no_save = cost_no_save[k];
                const uint64_t total_saving_cycles = (nbr_iter_save * save_cost_per_iter) + (load_cycles * k);
                const uint64_t total_no_save_cycles = nbr_iter_no_save * no_save_cost_per_iter;

                const double ratio = (double)total_no_save_cycles / (double)total_saving_cycles;


#if EAPP_BREAK_EVEN_CSV_OUTPUT
                {
                    emit_break_even_csv(runs, k, measured_compute, ratio);
                    if (total_no_save_cycles >= total_saving_cycles)
                    {
                        threshold_reached = 1;
                    }
                }
#else
                if (total_no_save_cycles >= total_saving_cycles) 
                {
                    print_metric("current computation: ", measured_compute);
                    print_indexed_metric("cost save_cycle    ", nbr_iter_save, save_cost_per_iter);
                    print_indexed_metric("cost no_save_cycle ", nbr_iter_no_save, no_save_cost_per_iter);
                    print_indexed_metric("cycle ratio ", k, ratio);
                    
                    // stop after first found
                    threshold_reached = 1;
                    break; 
                }
#endif
            }

            if (!threshold_reached)
            {
                print_indexed_metric("threshold not reached for k=", k, runs);
            }
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
