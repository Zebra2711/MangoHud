#pragma once
#ifndef MANGOHUD_LOGGING_H
#define MANGOHUD_LOGGING_H

#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <condition_variable>

#include "timing.hpp"

#include "overlay_params.h"

struct logData{
  double fps;
  float frametime;
  float cpu_load;
  float cpu_power;
  int cpu_mhz;
  int gpu_load;
  int cpu_temp;
  int gpu_temp;
  int gpu_core_clock;
  int gpu_mem_clock;
  int gpu_power;
  float gpu_vram_used;
  float ram_used;
  float swap_used;
  float process_rss;

  Clock::duration previous;
};

class Logger {
public:
  Logger(const overlay_params* in_params);

  void start_logging();
  void stop_logging();
  void logging();

  void try_log();

  bool is_active() const { return m_logging_on; }

  void wait_until_data_valid();
  void notify_data_valid();

  auto last_log_end() const noexcept { return m_log_end; }
  auto last_log_begin() const noexcept { return m_log_start; }

  const std::vector<logData>& get_log_data() const noexcept { return m_log_array; }
  void clear_log_data() noexcept { m_log_array.clear(); }

  void writeToFile();

  void upload_last_log();
  void upload_last_logs();
  void calculate_benchmark_data();
  std::string output_folder;
  const int64_t log_interval;
  const int64_t log_duration;
  bool autostart_init = false;

private:
  std::vector<logData> m_log_array;
  std::vector<std::string> m_log_files;
  Clock::time_point m_log_start;
  Clock::time_point m_log_end;
  bool m_logging_on;

  std::mutex m_values_valid_mtx;
  std::condition_variable m_values_valid_cv;
  bool m_values_valid;
};

extern std::unique_ptr<Logger> logger;

extern std::string os, cpu, gpu, ram, kernel, driver, cpusched;
extern bool sysInfoFetched;
extern double fps;
extern float frametime;
extern logData currentLogData;

std::string exec(std::string command);
void autostart_log(int sleep);

#endif //MANGOHUD_LOGGING_H
