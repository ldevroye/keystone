#include "host.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

bool test_done = false;

void tamper_checkpoint_blob(vector<uint8_t>& blob)
{
  if (blob.empty()) {
    return;
  }

  if (strcmp(TEST_TAMPER_MODE, "none") == 0) {
    return;
  }

  if (strcmp(TEST_TAMPER_MODE, "bitflip") == 0) {
    const size_t idx = blob.size() / 2;
    blob[idx] ^= 0x01;
  } else if (strcmp(TEST_TAMPER_MODE, "truncate") == 0) {
    blob.resize(blob.size() > 1 ? blob.size() - 1 : 0);
  } else if (strcmp(TEST_TAMPER_MODE, "last-byte") == 0) {
    blob.back() ^= 0x01;
  }
}

void host_print(const char* str)
{
#if HOST_LOGGING
  printf("[HOST] %s\n", str);
#endif
}

void update_timing_stats(timing_stats& stats, uint64_t value)
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

void print_timing_stats(const char* label, const timing_stats& stats)
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

void print_analysis_run_summary(uint64_t analysis_index)
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
