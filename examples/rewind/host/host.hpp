#pragma once

#include "edge/edge_call.h"
#include "host/keystone.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "host_test.hpp"

using namespace Keystone;
using namespace std;

#ifndef MAX_RUNS
#define MAX_RUNS 10
#endif

#ifndef EAPP_RUNS
#define EAPP_RUNS 50
#endif

#ifndef PERIOD
#define PERIOD 30ULL
#endif

#ifndef FAULT_RANDOMIZE_SEED
#define FAULT_RANDOMIZE_SEED 1
#endif

#ifndef SEED
#define SEED 0x6b656973746f6e68ULL
#endif

#ifndef EAPP_LOGGING
#define EAPP_LOGGING 0
#endif

#ifndef HOST_LOGGING
#define HOST_LOGGING 1
#endif

#ifndef HOST_TESTING
#define HOST_TESTING 0
#endif

#ifndef EAPP_TESTING
#define EAPP_TESTING 0
#endif

#ifndef EAPP_BLOB_SIZE_TESTING
#define EAPP_BLOB_SIZE_TESTING 0
#endif

#ifndef EAPP_CYCLE_BREAKDOWN_TESTING
#define EAPP_CYCLE_BREAKDOWN_TESTING 0
#endif

#ifndef EAPP_BREAK_EVEN_TESTING
#define EAPP_BREAK_EVEN_TESTING 0
#endif

#ifndef ANALYSIS_RUNS
#define ANALYSIS_RUNS 20
#endif

#ifndef STRINGIFY_IMPL
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#endif

enum
{
  OCALL_PRINT_BUFFER = 1,
  OCALL_LOAD_CHECKPOINT_BLOB = 8,
  OCALL_SAVE_CHECKPOINT_BLOB = 9,
};

inline timing_stats save_stats;
inline timing_stats load_stats;
inline timing_stats run_stats;
inline timing_stats blob_size_stats;
inline timing_stats retry_stats;
inline vector<uint8_t> saved_blob;

const auto success = Error::Success;

void host_print(const char* str);
void host_print_if_not_testing(const char* str);
void save_checkpoint_blob_dispatch(void* buffer);
void load_checkpoint_blob_dispatch(void* buffer);
void print_buffer_dispatch(void* buffer);
Error configure_enclave(Enclave& enclave, Params& params, char** argv);

inline void print_test_parameters()
{
#if HOST_TESTING || HOST_LOGGING
  char buffer[320];

  snprintf(buffer, sizeof(buffer), "test params:\n"
         "\thost_logging=%d\n"
         "\teapp_logging=%d\n"
         "\thost_testing=%d\n"
         "\teapp_testing=%d\n"
         "\thost_max_runs=%d\n"
         "\tanalysis_runs=%d\n"
         "\teapp_runs=%s\n"
         "\tfault_period=%s\n"
         "\tfault_randomize_seed=%s\n"
         "\tfault_seed=%s\n"
          "\teapp_blob_size_testing=%d\n"
          "\teapp_cycle_breakdown_testing=%d\n"
          "\teapp_break_even_testing=%d\n"
         "\ttamper_mode=%s",
                HOST_LOGGING,
#ifdef EAPP_LOGGING
                EAPP_LOGGING,
#else
                HOST_LOGGING,
#endif
                HOST_TESTING,
                EAPP_TESTING,
                MAX_RUNS,
                HOST_TESTING ? ANALYSIS_RUNS : 1,
                STRINGIFY(EAPP_RUNS),
                STRINGIFY(PERIOD),
                STRINGIFY(FAULT_RANDOMIZE_SEED),
                STRINGIFY(SEED),
                EAPP_BLOB_SIZE_TESTING,
                EAPP_CYCLE_BREAKDOWN_TESTING,
                EAPP_BREAK_EVEN_TESTING,
                TEST_TAMPER_MODE);
  host_print(buffer);
#endif
}
