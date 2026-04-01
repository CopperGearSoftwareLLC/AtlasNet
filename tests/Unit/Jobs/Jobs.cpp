#include "atlasnet/core/job/JobSystem.hpp"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <queue>
#include <thread>

using namespace std::chrono_literals;

TEST(JobsTests, JobExecutesCallable) {
  std::atomic<int> calls{0};

  AtlasNet::Job job([&](AtlasNet::Job::JobContext &) { ++calls; });
  job();

  EXPECT_EQ(calls.load(), 1);
}

TEST(JobsTests, PriorityQueueReturnsHighestPriorityFirst) {
  std::priority_queue<AtlasNet::Job, std::vector<AtlasNet::Job>> q;

  AtlasNet::Job low([](AtlasNet::Job::JobContext &) {});
  low.set_priority(AtlasNet::JobPriority::eLow);

  AtlasNet::Job high([](AtlasNet::Job::JobContext &) {});
  high.set_priority(AtlasNet::JobPriority::eHigh);

  q.push(low);
  q.push(high);

  EXPECT_EQ(q.top().get_priority(), AtlasNet::JobPriority::eHigh);
}

TEST(JobsTests, HandleCanRequestRepeatAndChangePriority) {
  AtlasNet::Job job([](AtlasNet::Job::JobContext &h) {
    h.set_priority(AtlasNet::JobPriority::eVeryHigh).repeat_once(25ms);
  });

  job();

  EXPECT_EQ(job.get_priority(), AtlasNet::JobPriority::eVeryHigh);
  EXPECT_TRUE(job.should_repeat());
  EXPECT_EQ(job.get_repeat_interval(), 25ms);
}

TEST(JobsTests, RepeatingPresetRequestsRepeatOnRun) {
  AtlasNet::Job job([](AtlasNet::Job::JobContext &) {});
  job.repeating(15ms);

  job();

  EXPECT_TRUE(job.should_repeat());
  EXPECT_EQ(job.get_repeat_interval(), 15ms);
}

TEST(JobsTests, JobSystemRunsPushedJob) {
  AtlasNet::JobSystem system;

  std::promise<void> done;
  auto doneFuture = done.get_future();

  AtlasNet::Job job([&](AtlasNet::Job::JobContext &) { done.set_value(); });
  system.PushJob(job);

  EXPECT_EQ(doneFuture.wait_for(1s), std::future_status::ready);

  system.Shutdown();
}

TEST(JobsTests, JobSystemRepeatingJobRunsMoreThanOnce) {
  AtlasNet::JobSystem system;

  std::atomic<int> calls{0};
  std::atomic<bool> signaled{false};

  std::promise<void> done;
  auto doneFuture = done.get_future();

  AtlasNet::Job repeatingJob([&](AtlasNet::Job::JobContext &) {
    const int n = ++calls;
    if (n >= 2 && !signaled.exchange(true)) {
      done.set_value();
    }
  });
  repeatingJob.repeating(10ms);

  system.PushJob(repeatingJob);

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);
  EXPECT_GE(calls.load(), 2);

  system.Shutdown();
}

TEST(JobsTests, JobSystemBruteForceManyJobsAllExecute) {
  AtlasNet::JobSystem system;

  constexpr int kJobCount = 20000;
  std::atomic<int> completed{0};
  std::atomic<bool> doneSignaled{false};

  std::promise<void> done;
  auto doneFuture = done.get_future();

  for (int i = 0; i < kJobCount; ++i) {
    AtlasNet::Job job([&](AtlasNet::Job::JobContext &) {
      const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
      if (n == kJobCount && !doneSignaled.exchange(true)) {
        done.set_value();
      }
    });

    // Optional small priority variation to exercise queue ordering paths
    if ((i % 3) == 0) {
      job.set_priority(AtlasNet::JobPriority::eHigh);
    } else if ((i % 3) == 1) {
      job.set_priority(AtlasNet::JobPriority::eMedium);
    } else {
      job.set_priority(AtlasNet::JobPriority::eLow);
    }

    system.PushJob(job);
  }

  EXPECT_EQ(doneFuture.wait_for(10s), std::future_status::ready);
  EXPECT_EQ(completed.load(std::memory_order_relaxed), kJobCount);

  system.Shutdown();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}