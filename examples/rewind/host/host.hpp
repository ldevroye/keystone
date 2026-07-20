#pragma once

#include "edge/edge_call.h"
#include "host/keystone.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

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

#ifndef ENABLE_TESTING
#define ENABLE_TESTING 0
#endif

#ifndef ANALYSIS_RUNS
#define ANALYSIS_RUNS 1000
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

struct timing_stats
{
  uint64_t count = 0;
  uint64_t sum = 0;
  uint64_t min = 0;
  uint64_t max = 0;
};

inline timing_stats save_stats;
inline timing_stats load_stats;
inline timing_stats run_stats;
inline vector<uint8_t> saved_checkpoint_blob;

inline void host_print(const char* str)
{
  printf("[HOST] %s\n", str);
}

inline void host_print_if_not_testing(const char* str)
{
#if HOST_LOGGING && !ENABLE_TESTING
  host_print(str);
#else
  (void)str;
#endif
}

inline void update_timing_stats(timing_stats& stats, uint64_t value)
{
  if (stats.count == 0)
  {
    stats.min = value;
    stats.max = value;
  }
  else
  {
    if (value < stats.min) stats.min = value;
    if (value > stats.max) stats.max = value;
  }

  stats.sum += value;
  stats.count++;
}

inline void print_timing_stats(const char* label, const timing_stats& stats)
{
  if (stats.count == 0)
  {
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "%s count=0", label);
    host_print(buffer);
    return;
  }

  const double avg = static_cast<double>(stats.sum) / static_cast<double>(stats.count);
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%s count=%llu avg=%.2f min=%llu max=%llu",
                label,
                static_cast<unsigned long long>(stats.count),
                avg,
                static_cast<unsigned long long>(stats.min),
                static_cast<unsigned long long>(stats.max));
  host_print(buffer);
}

inline void print_analysis_run_summary(uint64_t analysis_index)
{
  if (run_stats.count == 0)
  {
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "analysis_run=%llu count=0", static_cast<unsigned long long>(analysis_index));
    host_print(buffer);
    return;
  }

  const double avg = static_cast<double>(run_stats.sum) / static_cast<double>(run_stats.count);
  char buffer[160];
  snprintf(buffer, sizeof(buffer), "analysis_run=%llu count=%llu avg=%.2f min=%llu max=%llu",
                static_cast<unsigned long long>(analysis_index),
                static_cast<unsigned long long>(run_stats.count),
                avg,
                static_cast<unsigned long long>(run_stats.min),
                static_cast<unsigned long long>(run_stats.max));
  host_print(buffer);
}

inline void print_test_parameters()
{
#if ENABLE_TESTING || HOST_LOGGING
  char buffer[320];
  snprintf(buffer, sizeof(buffer), "test params:\n"
         "\thost_logging=%d\n"
         "\teapp_logging=%d\n"
         "\thost_max_runs=%d\n"
         "\tanalysis_runs=%d\n"
         "\teapp_runs=%s\n"
         "\tfault_period=%s\n"
         "\tfault_randomize_seed=%s\n"
         "\tfault_seed=%s",
                HOST_LOGGING,
#ifdef EAPP_LOGGING
                EAPP_LOGGING,
#else
                HOST_LOGGING,
#endif
                MAX_RUNS,
                ENABLE_TESTING ? ANALYSIS_RUNS : 1,
                STRINGIFY(EAPP_RUNS),
                STRINGIFY(PERIOD),
                STRINGIFY(FAULT_RANDOMIZE_SEED),
                STRINGIFY(SEED));
  host_print(buffer);
#endif
}

void save_checkpoint_blob_dispatch(void* buffer);
void load_checkpoint_blob_dispatch(void* buffer);
void print_buffer_dispatch(void* buffer);
Error configure_enclave(Enclave& enclave, Params& params, char** argv);
