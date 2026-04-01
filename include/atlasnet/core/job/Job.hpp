#pragma once

#include "atlasnet/core/job/JobEnums.hpp"
#include "boost/container/small_vector.hpp"
#include <chrono>
#include <concepts>
#include <functional>
#include <iostream>
#include <string_view>
#include <utility>

namespace AtlasNet
{

class Job
{
public:
  class JobContext
  {
  public:
    explicit JobContext(Job& job) noexcept : job_(job) {}

    [[nodiscard]] JobPriority get_priority() const noexcept
    {
      return job_.get_priority();
    }

    JobContext& set_priority(JobPriority priority) noexcept
    {
      job_.set_priority(priority);
      return *this;
    }

    JobContext& repeat_once(std::chrono::milliseconds interval =
                               std::chrono::milliseconds::zero()) noexcept
    {
      job_.request_repeat(interval);
      return *this;
    }
    JobContext& on_complete(Job onCompleteJob) noexcept
    {
      job_.on_complete(std::move(onCompleteJob));
      return *this;
    }
    JobContext& set_name(std::string name) noexcept
    {
      job_.set_name(std::move(name));
      return *this;
    }
    JobContext& set_notify_level(JobNotifyLevel notify) noexcept
    {
      job_.set_notify_level(notify);
      return *this;
    }

    const std::optional<std::string_view> get_name() const noexcept
    {
      return job_.get_name();
    }

  private:
    Job& job_;
  };

  using JobFunc = std::function<void(JobContext&)>;

  explicit Job(JobFunc func) : func_(std::move(func)) {}

  Job& repeating(std::chrono::milliseconds interval) noexcept
  {
    repeating_ = true;
    repeating_interval_ = interval;
    return *this;
  }

  Job& set_priority(JobPriority priority) noexcept
  {
    priority_ = priority;
    return *this;
  }
  Job& set_name(std::string name) noexcept
  {
    JobName = std::move(name);
    return *this;
  }
  Job& on_complete(Job onCompleteJob) noexcept
  {
    OnCompleteJobs.push_back(std::move(onCompleteJob));
    return *this;
  }
  Job& set_notify_level(JobNotifyLevel notify) noexcept
  {
    notify_ = notify;
    return *this;
  }

  std::optional<std::string_view> get_name() const noexcept
    {
      return JobName ? std::optional<std::string_view>(*JobName) : std::nullopt;
    }

  [[nodiscard]] JobPriority get_priority() const noexcept
  {
    return priority_;
  }
  [[nodiscard]] JobNotifyLevel get_notify_level() const noexcept
  {
    return notify_;
  }
  [[nodiscard]] bool should_repeat() const noexcept
  {
    return repeat_requested_;
  }

  [[nodiscard]] std::chrono::milliseconds get_repeat_interval() const noexcept
  {
    return repeat_interval_;
  }

  [[nodiscard]] bool operator<(const Job& other) const noexcept
  {
    return priority_ < other.priority_;
  }

  Job& operator()()
  {
    repeat_requested_ = repeating_;
    if (repeating_)
    {
      repeat_interval_ = repeating_interval_;
    }

    JobContext handle(*this);
    func_(handle);
    return *this;
  }

private:
  void request_repeat(std::chrono::milliseconds interval) noexcept
  {
    repeat_requested_ = true;
    repeat_interval_ = interval;
  }

  JobFunc func_;
  std::optional<std::string> JobName;
  JobPriority priority_ = JobPriority::eMedium;
  JobNotifyLevel notify_ = JobNotifyLevel::eNone;
  bool repeating_ = false;
  bool repeat_requested_ = false;
  std::chrono::milliseconds repeating_interval_ =
      std::chrono::milliseconds::zero();
  std::chrono::milliseconds repeat_interval_ =
      std::chrono::milliseconds::zero();
  std::vector<Job> OnCompleteJobs;
};

} // namespace AtlasNet
