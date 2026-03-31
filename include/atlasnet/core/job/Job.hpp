#pragma once

#include <chrono>
#include <concepts>
#include <functional>
#include <iostream>
#include <utility>

namespace AtlasNet {

enum JobPriority {
  eVeryLow = 1,
  eLow = 2,
  eMediumLow = 3,
  eMedium = 4,
  eMediumHigh = 5,
  eHigh = 6,
  eVeryHigh = 7,
  eCritical = 8,
  eEmergency = 9
};

class Job {
public:
  class JobHandle {
  public:
    explicit JobHandle(Job &job) noexcept : job_(job) {}

    [[nodiscard]] JobPriority get_priority() const noexcept {
      return job_.get_priority();
    }

    JobHandle &set_priority(JobPriority priority) noexcept {
      job_.set_priority(priority);
      return *this;
    }

    JobHandle &repeat_once(
        std::chrono::milliseconds interval =
            std::chrono::milliseconds::zero()) noexcept {
      job_.request_repeat(interval);
      return *this;
    }

  private:
    Job &job_;
  };

  using JobFunc = std::function<void(JobHandle &)>;

  template <typename Func>
  requires std::constructible_from<JobFunc, Func>
  explicit Job(Func &&func) : func_(std::forward<Func>(func)) {}

  Job &repeating(std::chrono::milliseconds interval) noexcept {
    repeating_ = true;
    repeating_interval_ = interval;
    return *this;
  }

  Job &set_priority(JobPriority priority) noexcept {
    priority_ = priority;
    return *this;
  }

  [[nodiscard]] JobPriority get_priority() const noexcept { return priority_; }

  [[nodiscard]] bool should_repeat() const noexcept { return repeat_requested_; }

  [[nodiscard]] std::chrono::milliseconds get_repeat_interval() const noexcept {
    return repeat_interval_;
  }

  [[nodiscard]] bool operator<(const Job &other) const noexcept {
    return priority_ < other.priority_;
  }

  Job &operator()() {
    repeat_requested_ = repeating_;
    if (repeating_) {
      repeat_interval_ = repeating_interval_;
    }

    JobHandle handle(*this);
    func_(handle);
    return *this;
  }

private:
  void request_repeat(std::chrono::milliseconds interval) noexcept {
    repeat_requested_ = true;
    repeat_interval_ = interval;
  }

  JobFunc func_;
  JobPriority priority_ = eMedium;
  bool repeating_ = false;
  bool repeat_requested_ = false;
  std::chrono::milliseconds repeating_interval_ =
      std::chrono::milliseconds::zero();
  std::chrono::milliseconds repeat_interval_ = std::chrono::milliseconds::zero();
};


void test()
{

  Job([](Job::JobHandle& h){
    std::cout << " hi\n";
  });
}
} // namespace AtlasNet
