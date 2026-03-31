#pragma once

#include "atlasnet/core/Singleton.hpp"
#include "atlasnet/core/System.hpp"
#include "atlasnet/core/job/Job.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace AtlasNet::Jobflow {

class JobSystem : public System<JobSystem> {
  using Clock = std::chrono::steady_clock;

  struct DelayedJob {
    Clock::time_point due_at;
    AtlasNet::Job job;

    // min-heap behavior using std::priority_queue
    bool operator<(const DelayedJob &other) const {
      return due_at > other.due_at;
    }
  };

  std::priority_queue<AtlasNet::Job, std::vector<AtlasNet::Job>> jobQueue_;
  std::priority_queue<DelayedJob> delayedJobs_;

  std::vector<std::thread> workers_;
  std::mutex mutex_;
  std::condition_variable cv_;

  const uint16_t maxConcurrentJobs_ =
      std::thread::hardware_concurrency() > 0
          ? static_cast<uint16_t>(std::thread::hardware_concurrency())
          : 4;

  std::atomic<bool> shutdownInitiated_ = false;

public:
  JobSystem() {
    std::cout << "JobSystem initialized with " << maxConcurrentJobs_
              << " worker threads." << std::endl;
    workers_.reserve(maxConcurrentJobs_);
    for (uint16_t i = 0; i < maxConcurrentJobs_; ++i) {
      workers_.emplace_back(&JobSystem::WorkerThread, this);
    }
  }

  ~JobSystem() {
    Shutdown();
    for (auto &worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  void PushJob(const AtlasNet::Job &job) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      jobQueue_.push(job);
    }
    cv_.notify_one();
  }

  /* void PushDelayedJob(const AtlasNet::Job& job,
                      std::chrono::milliseconds delay) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      delayedJobs_.push(DelayedJob{Clock::now() + delay, job});
    }
    cv_.notify_one();
  } */

  void Shutdown() override {
    shutdownInitiated_.store(true, std::memory_order_relaxed);
    cv_.notify_all();
  }

private:
  void PromoteExpiredDelayedJobsLocked() {
    const auto now = Clock::now();

    while (!delayedJobs_.empty() && delayedJobs_.top().due_at <= now) {
      jobQueue_.push(std::move(delayedJobs_.top().job));
      delayedJobs_.pop();
    }
  }

  void WorkerThread() {
    while (true) {
      std::optional<AtlasNet::Job> jobToRun;

      {
        std::unique_lock<std::mutex> lock(mutex_);

        while (true) {
          if (shutdownInitiated_.load(std::memory_order_relaxed)) {
            return;
          }

          // Move due delayed jobs into the runnable queue
          PromoteExpiredDelayedJobsLocked();

          // If there is runnable work, grab it
          if (!jobQueue_.empty()) {
            jobToRun = std::move(jobQueue_.top());
            jobQueue_.pop();
            break;
          }

          // No runnable work; sleep until next delayed job expires
          if (!delayedJobs_.empty()) {
            cv_.wait_until(lock, delayedJobs_.top().due_at);
          } else {
            cv_.wait(lock);
          }
        }
      }

      // Run outside the lock
      if (jobToRun) {
        (*jobToRun)();

        const auto repeatInterval = jobToRun->get_repeat_interval();
        if (jobToRun->should_repeat() &&
            repeatInterval > std::chrono::milliseconds::zero()) {
          std::lock_guard<std::mutex> lock(mutex_);
          delayedJobs_.push(
              DelayedJob{Clock::now() + repeatInterval, std::move(*jobToRun)});
          cv_.notify_one();
        }
      }
    }
  }
};

} // namespace AtlasNet::Jobflow