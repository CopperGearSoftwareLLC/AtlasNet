#pragma once

#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobRuntime.hpp"


namespace AtlasNet
{
class JobHandle
{
  std::shared_ptr<job_runtime> runtime;

public:
  JobHandle(std::shared_ptr<job_runtime> runtime) : runtime(std::move(runtime))
  {
  }

  JobState get_state() const
  {
    return runtime->state.load(std::memory_order_acquire);
  }

  bool is_completed() const
  {
    return get_state() == JobState::eCompleted;
  }
  bool is_running() const
  {
    return get_state() == JobState::eRunning;
  }
  bool is_pending() const
  {
    return get_state() == JobState::ePending;
  }

  void wait() const
  {
    if (!runtime)
      return;

    if (is_completed())
      return;

    std::unique_lock lock(runtime->mutex);
    runtime->cv.wait(lock,
                     [this]
                     {
                       return runtime->state.load(std::memory_order_acquire) ==
                              JobState::eCompleted;
                     });
  }

  bool wait_for(std::chrono::milliseconds timeout) const
  {
    if (!runtime)
      return true;

    if (is_completed())
      return true;

    std::unique_lock lock(runtime->mutex);
    return runtime->cv.wait_for(lock, timeout,
                                [this]
                                {
                                  return runtime->state.load(
                                             std::memory_order_acquire) ==
                                         JobState::eCompleted;
                                });
  }

  void on_complete(Job onCompleteJob) const;
};
} // namespace AtlasNet