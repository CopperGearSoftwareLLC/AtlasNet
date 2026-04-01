#pragma once


#include "atlasnet/core/job/Job.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include <condition_variable>
#include <mutex>
namespace AtlasNet {


struct job_runtime
{
  Job job;
  job_runtime(const Job& job) : job(job), state(JobState::ePending) {}
  std::atomic<JobState> state{JobState::ePending};
    mutable std::mutex mutex;
  mutable std::condition_variable cv;

  bool operator<(const job_runtime& other) const
  {
    return job.get_priority() < other.job.get_priority();
  }
};
}