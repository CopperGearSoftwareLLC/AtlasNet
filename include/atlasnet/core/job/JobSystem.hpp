#pragma once

#include "atlasnet/core/Singleton.hpp"
#include "atlasnet/core/System.hpp"
#include "atlasnet/core/job/Job.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobHandle.hpp"
#include "atlasnet/core/job/JobRuntime.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace AtlasNet
{

struct JobRuntimeCompare
{
  bool operator()(const std::shared_ptr<job_runtime>& a,
                  const std::shared_ptr<job_runtime>& b) const
  {
    return a->job.get_priority() > b->job.get_priority();
  }
};

class JobView
{
};
class JobSystem : public System<JobSystem>
{
  using Clock = std::chrono::steady_clock;

  struct DelayedJob
  {
    Clock::time_point due_at;
    std::shared_ptr<job_runtime> job;

    // min-heap behavior using std::priority_queue
    bool operator<(const DelayedJob& other) const
    {
      return due_at > other.due_at;
    }
  };

  std::priority_queue<std::shared_ptr<job_runtime>,
                      std::vector<std::shared_ptr<job_runtime>>,
                      JobRuntimeCompare>
      jobQueue_;
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
  JobSystem()
  {
    std::cout << "JobSystem initialized with " << maxConcurrentJobs_
              << " worker threads." << std::endl;
    workers_.reserve(maxConcurrentJobs_);
    for (uint16_t i = 0; i < maxConcurrentJobs_; ++i)
    {
      workers_.emplace_back(&JobSystem::WorkerThread, this);
    }
  }

  ~JobSystem()
  {
    Shutdown();
    for (auto& worker : workers_)
    {
      if (worker.joinable())
      {
        worker.join();
      }
    }
  }

  JobHandle PushJob(const AtlasNet::Job& job)
  {
      std::shared_ptr<job_runtime> jobRuntime = std::make_shared<job_runtime>(job);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      jobQueue_.push(jobRuntime);
    }
    cv_.notify_one();
    return JobHandle(jobRuntime);
  }

  /* void PushDelayedJob(const AtlasNet::Job& job,
                      std::chrono::milliseconds delay) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      delayedJobs_.push(DelayedJob{Clock::now() + delay, job});
    }
    cv_.notify_one();
  } */

  void Shutdown() override
  {
    shutdownInitiated_.store(true, std::memory_order_relaxed);
    cv_.notify_all();
  }

private:
  void PromoteExpiredDelayedJobsLocked()
  {
    const auto now = Clock::now();

    while (!delayedJobs_.empty() && delayedJobs_.top().due_at <= now)
    {
      jobQueue_.push(std::move(delayedJobs_.top().job));
      delayedJobs_.pop();
    }
  }

  void WorkerThread()
  {
    while (true)
    {
      std::optional<std::shared_ptr<job_runtime>> jobToRun;

      {
        std::unique_lock<std::mutex> lock(mutex_);

        while (true)
        {
          if (shutdownInitiated_.load(std::memory_order_relaxed))
          {
            return;
          }

          // Move due delayed jobs into the runnable queue
          PromoteExpiredDelayedJobsLocked();

          // If there is runnable work, grab it
          if (!jobQueue_.empty())
          {
            jobToRun = std::move(jobQueue_.top());
            jobQueue_.pop();
            break;
          }

          // No runnable work; sleep until next delayed job expires
          if (!delayedJobs_.empty())
          {
            cv_.wait_until(lock, delayedJobs_.top().due_at);
          }
          else
          {
            cv_.wait(lock);
          }
        }
      }

      // Run outside the lock
      if (jobToRun)
      {
        std::unique_lock lock(jobToRun->get()->mutex);
        if (jobToRun->get()->job.get_notify_level() == JobNotifyLevel::eOnStart)
        {
          std::cout << "Starting job "
                    << jobToRun->get()->job.get_name().value_or("<unnamed>")
                    << " on thread " << std::this_thread::get_id() << '\n';
        }
        jobToRun->get()->state.store(JobState::eRunning, std::memory_order_release);

        (jobToRun->get()->job)();
        jobToRun->get()->state.store(JobState::eCompleted, std::memory_order_release);
        jobToRun->get()->cv.notify_all();
        if (jobToRun->get()->job.get_notify_level() == JobNotifyLevel::eOnComplete)
        {
          std::cout << "Completed job "
                    << jobToRun->get()->job.get_name().value_or("<unnamed>")
                    << " on thread " << std::this_thread::get_id() << '\n';
        }

        const auto repeatInterval = jobToRun->get()->job.get_repeat_interval();
        if (jobToRun->get()->job.should_repeat() &&
            repeatInterval > std::chrono::milliseconds::zero())
        {
          std::lock_guard<std::mutex> lock(mutex_);
          delayedJobs_.push(
              DelayedJob{Clock::now() + repeatInterval, std::move(*jobToRun)});
          cv_.notify_one();
        }
      }
    }
  }
};

} // namespace AtlasNet