#pragma once
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mesa/util/os_time.h>
#include <numeric>
#include <mutex>
#include <algorithm>
#include <condition_variable>
#include <stdexcept>
#include <iomanip>
#include <spdlog/spdlog.h>

struct metric_t {
    std::string name;
    float value;
    std::string display_name;
};

class fpsMetrics {
    private:
        std::vector<std::pair<uint64_t, float>> fps_stats;
        std::thread thread;
        std::mutex mtx;
        std::condition_variable cv;
        bool run = false;
        bool thread_init = false;
        bool terminate = false;
        bool resetting = false;

        void _thread() {
            thread_init = true;
            while (true){
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this] { return run; });

                if (terminate)
                    break;

                calculate();

                run = false;
            }
        }

        void calculate(){
            std::vector<float> sorted_values;
            for (const auto& p : fps_stats)
                sorted_values.push_back(p.second);

            std::sort(sorted_values.begin(), sorted_values.end());

            auto it = metrics.begin();
            while (it != metrics.end()) {
                if (it->name == "AVG") {
                    it->display_name = it->name;
                    if (!fps_stats.empty()) {
                        float sum = std::accumulate(fps_stats.begin(), fps_stats.end(), 0.0f,
                                                    [](float acc, const std::pair<uint64_t, float>& p) {
                                                        return acc + 1000.f / p.second;
                                                    });
                        it->value = 1000.f / (sum / fps_stats.size());
                    }
                } else {
                    try {
                        float val = std::stof(it->name);
                        if (val <= 0 || val >= 1 ) {
                            SPDLOG_DEBUG("Failed to use fps metric, it's out of range {}", it->name);
                            it = metrics.erase(it);
                            break;
                        }
                        float multiplied_val = val * 100;
                        std::ostringstream stream;
                        if (multiplied_val == static_cast<int>(multiplied_val)) {
                            stream << std::fixed << std::setprecision(0) << multiplied_val << "%";
                        } else {
                            stream << std::fixed << std::setprecision(1) << multiplied_val << "%";
                        }
                        it->display_name = stream.str();
                        uint64_t idx = val * sorted_values.size() - 1;
                        if (idx >= sorted_values.size())
                            break;

                        it->value = sorted_values[idx];
                    } catch (const std::invalid_argument& e) {
                        SPDLOG_DEBUG("Failed to use fps metric value {}", it->name);
                        it = metrics.erase(it);
                    }
                }
                ++it;
            }

        }

        std::vector<metric_t> add_metrics_to_vector(std::vector<std::string> values) {
            std::vector<metric_t> _metrics;
            for (auto& val : values){
                for(char& c : val) {
                    c = std::toupper(static_cast<unsigned char>(c));
                }
                _metrics.push_back({val, 0.0f});
            }
            return _metrics;
        }

    public:
        std::vector<metric_t> metrics;

        fpsMetrics(std::vector<std::string> values){
            metrics = add_metrics_to_vector(values);

            if (!thread_init)
                thread = std::thread(&fpsMetrics::_thread, this);
        };

        fpsMetrics(std::vector<std::string> values, std::vector<float> only_fps) {
            metrics = add_metrics_to_vector(values);
            for (auto& fps : only_fps)
                fps_stats.push_back({0, fps});

            calculate();
        };

        void update(uint64_t now, double fps){
            if (resetting)
                return;

            if (fps > 0.0001)
                fps_stats.push_back({now, fps});

            uint64_t one_minute_duration = 1 * 60ULL * 1000000000ULL; // 60000000000 ns

            // Check if the system's uptime is less than 1 minutes
            if (now >= one_minute_duration) {
                uint64_t one_minutes_ago = now - one_minute_duration;

                fps_stats.erase(
                    std::remove_if(
                        fps_stats.begin(),
                        fps_stats.end(),
                        [one_minutes_ago](const std::pair<uint64_t, float>& entry) {
                            return entry.first < one_minutes_ago;
                        }
                    ),
                    fps_stats.end()
                );
            }
        }

        void update_thread(){
            if (resetting)
                return;

            {
                std::lock_guard<std::mutex> lock(mtx);
                run = true;
            }
            cv.notify_one();
        }

        void reset_metrics(){
            resetting = true;
            while (run){}
            fps_stats.clear();
            resetting = false;
        }

        ~fpsMetrics(){
            terminate = true;
            {
                std::lock_guard<std::mutex> lock(mtx);
                run = true;
            }
            cv.notify_one();
            if (thread.joinable())
                thread.join();
        }
};

extern std::unique_ptr<fpsMetrics> fpsmetrics;