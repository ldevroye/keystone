#pragma once

#include <cstdint>
#include <vector>

#ifndef TEST_TAMPER_MODE
#define TEST_TAMPER_MODE "none"
#endif

using namespace std;

struct timing_stats
{
  uint64_t count = 0;
  uint64_t sum = 0;
  uint64_t min = 0;
  uint64_t max = 0;
};

extern bool test_done;

void tamper_checkpoint_blob(vector<uint8_t>& blob);
void host_print(const char* str);
void host_print_if_not_testing(const char* str);
void update_timing_stats(timing_stats& stats, uint64_t value);
void print_timing_stats(const char* label, const timing_stats& stats);
void print_analysis_run_summary(uint64_t analysis_index);
