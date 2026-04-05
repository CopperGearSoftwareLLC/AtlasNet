#include "atlasnet/core/job/JobContext.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobOptions.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(Jobs, SubmitRunsCallable)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::atomic<int> calls{0};

  auto handle = system.Submit([&](AtlasNet::JobContext&) { ++calls; },
                              AtlasNet::JobOpts::Name{"SubmitRunsCallable"});

  handle.wait();

  EXPECT_EQ(calls.load(), 1);

  system.Shutdown();
}
TEST(jobs, RepeatingTask)
{
  std::atomic_uint16_t val;
  using namespace AtlasNet;

  JobSystem system(JobSystem::Config{});
  std::mutex mtx;
  std::condition_variable cv;
  auto handle = system.Submit(
      [&](AtlasNet::JobContext& ctx)
      {
        val++;
        if (val < 5)
        {
          std::cout << "Requesting repeat from context, val = " << val.load()
                    << std::endl;
          ctx.repeat_once(10ms);
        }
        std::lock_guard lock(mtx);
        cv.notify_all();
      },
      AtlasNet::JobOpts::Name{"RepeatingTask"});

  std::unique_lock lock(mtx);
  cv.wait_for(lock, 2s, [&] { return val.load() >= 5; });
  handle.cancel();
  EXPECT_GE(val.load(), 5);
};

TEST(Jobs, HigherPriorityRunsFirst)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{1}); // single worker

  constexpr int Runs = 20;

  enum class JobType
  {
    eNone,
    eLow,
    eHigh
  };

  std::array<JobType, Runs> order;
  order.fill(JobType::eNone);

  std::atomic<int> lowRun{0};
  std::atomic<int> highRun{0};

  auto low = system.Submit(
      [&](AtlasNet::JobContext& ctx)
      {
        int run = lowRun.fetch_add(1);
        if (run >= Runs)
        {
          return;
        }

        if (run < Runs - 1)
        {
          ctx.repeat_once();
        }

        // Try to claim this slot as Low only if nobody claimed it yet.
        JobType expected = JobType::eNone;
        std::atomic_ref<JobType> slot(order[run]);
        slot.compare_exchange_strong(expected, JobType::eLow);
      },
      AtlasNet::JobOpts::Name{"Low"},
      AtlasNet::JobOpts::TPriority<AtlasNet::JobPriority::eLow>{});

  auto high = system.Submit(
      [&](AtlasNet::JobContext& ctx)
      {
        int run = highRun.fetch_add(1);
        if (run >= Runs)
        {
          return;
        }

        if (run < Runs - 1)
        {
          ctx.repeat_once();
        }

        // Try to claim this slot as High only if nobody claimed it yet.
        JobType expected = JobType::eNone;
        std::atomic_ref<JobType> slot(order[run]);
        slot.compare_exchange_strong(expected, JobType::eHigh);
      },
      AtlasNet::JobOpts::Name{"High"},
      AtlasNet::JobOpts::TPriority<AtlasNet::JobPriority::eHigh>{});

  low.wait();
  high.wait();

  int lowWins = 0;
  int highWins = 0;
  int noneWins = 0;

  for (JobType v : order)
  {
    switch (v)
    {
    case JobType::eLow:
      ++lowWins;
      break;
    case JobType::eHigh:
      ++highWins;
      break;
    case JobType::eNone:
      ++noneWins;
      break;
    }
  }

  EXPECT_EQ(noneWins, 0);
  EXPECT_EQ(lowWins + highWins, Runs);

  // Main property we care about:
  EXPECT_GT(highWins, lowWins);

  system.Shutdown();
}
TEST(Jobs, HandleCanRequestRepeat)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::atomic<int> calls{0};
  std::promise<void> done;
  auto doneFuture = done.get_future();
  std::atomic<bool> signaled{false};

  auto handle = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        int n = ++calls;
        if (n >= 2 && !signaled.exchange(true))
        {
          done.set_value();
        }
      },
      AtlasNet::JobOpts::Name{"HandleCanRequestRepeat"});

  handle.request_repeat(25ms);

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);
  EXPECT_GE(calls.load(), 2);

  system.Shutdown();
}

TEST(Jobs, RepeatFromContextRunsMoreThanOnce)
{
  using namespace std::chrono_literals;
  using namespace AtlasNet;

  JobSystem system(JobSystem::Config{});

  std::atomic<int> calls{0};
  std::promise<void> done;
  auto doneFuture = done.get_future();
  std::atomic<bool> signaled{false};

  auto handle = system.Submit(
      [&](AtlasNet::JobContext& ctx)
      {
        std::cout << "Job run " << (calls.load() + 1) << std::endl;
        int n = ++calls;
        if (n < 2)
        {
          std::cout << "Requesting repeat from context\n";

          ctx.repeat_once(10ms);
        }
        if (n >= 2 && !signaled.exchange(true))
        {
          done.set_value();
        }
      },
      AtlasNet::JobOpts::Name{"RepeatFromContextRunsMoreThanOnce"});

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);

  handle.wait(); // Wait for terminal state, not just callback progress.

  EXPECT_GE(calls.load(), 2);
  EXPECT_TRUE(handle.is_completed());

  system.Shutdown();
}

TEST(Jobs, RepeatOptionRequestsAnotherRun)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::atomic<int> calls{0};
  std::promise<void> done;
  auto doneFuture = done.get_future();
  std::atomic<bool> signaled{false};

  auto handle = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        int n = ++calls;
        if (n >= 2 && !signaled.exchange(true))
        {
          done.set_value();
        }
      },
      AtlasNet::JobOpts::Name{"RepeatOptionRequestsAnotherRun"},
      AtlasNet::JobOpts::RepeatOnce{10ms});

  EXPECT_EQ(doneFuture.wait_for(2s), std::future_status::ready);
  EXPECT_GE(calls.load(), 2);

  system.Shutdown();
}

TEST(Jobs, ManyJobsAllExecute)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  constexpr int kJobCount = 20000;
  std::atomic<int> completed{0};
  std::atomic<bool> doneSignaled{false};

  std::promise<void> done;
  auto doneFuture = done.get_future();

  std::vector<JobHandle> handles;
  handles.reserve(kJobCount);

  for (int i = 0; i < kJobCount; ++i)
  {
    if ((i % 3) == 0)
    {
      handles.push_back(system.Submit(
          [&](AtlasNet::JobContext&)
          {
            const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == kJobCount && !doneSignaled.exchange(true))
            {
              done.set_value();
            }
          },
          AtlasNet::JobOpts::TPriority<AtlasNet::JobPriority::eHigh>{}));
    }
    else if ((i % 3) == 1)
    {
      handles.push_back(system.Submit(
          [&](AtlasNet::JobContext&)
          {
            const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == kJobCount && !doneSignaled.exchange(true))
            {
              done.set_value();
            }
          },
          AtlasNet::JobOpts::TPriority<AtlasNet::JobPriority::eMedium>{}));
    }
    else
    {
      handles.push_back(system.Submit(
          [&](AtlasNet::JobContext&)
          {
            const int n = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == kJobCount && !doneSignaled.exchange(true))
            {
              done.set_value();
            }
          },
          AtlasNet::JobOpts::TPriority<AtlasNet::JobPriority::eLow>{}));
    }
  }

  EXPECT_EQ(doneFuture.wait_for(10s), std::future_status::ready);
  EXPECT_EQ(completed.load(std::memory_order_relaxed), kJobCount);

  system.Shutdown();
}

TEST(Jobs, CompletionCascade)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::mutex mtx;
  std::condition_variable cv;

  int value = 0;
  bool done = false;

  auto handle = system.Submit([&](AtlasNet::JobContext&) { value = 1; },
                              AtlasNet::JobOpts::Name{"Level1"},
                              AtlasNet::JobOpts::OnComplete(
                                  [&](AtlasNet::JobContext&) { value = 2; },
                                  AtlasNet::JobOpts::Name{"Level2"},
                                  AtlasNet::JobOpts::OnComplete(
                                      [&](AtlasNet::JobContext&) { value = 3; },
                                      AtlasNet::JobOpts::Name{"Level3"},
                                      AtlasNet::JobOpts::OnComplete(
                                          [&](AtlasNet::JobContext&)
                                          {
                                            value = 4;
                                            {
                                              std::lock_guard lock(mtx);
                                              done = true;
                                            }
                                            cv.notify_one();
                                          },
                                          AtlasNet::JobOpts::Name{"Level4"}))));

  {
    std::unique_lock lock(mtx);
    EXPECT_TRUE(cv.wait_for(lock, 2s, [&] { return done; }));
  }

  handle.wait();
  EXPECT_EQ(value, 4);
  EXPECT_TRUE(done);

  system.Shutdown();
}

TEST(Jobs, HandleWaitObservesCompletion)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::atomic<bool> ran{false};

  auto handle = system.Submit([&](JobContext&) { ran = true; },
                              JobOpts::Name{"HandleWaitObservesCompletion"});

  handle.wait();

  EXPECT_TRUE(ran.load());
  EXPECT_TRUE(handle.is_completed());
  EXPECT_FALSE(handle.is_failed());

  system.Shutdown();
}

TEST(Jobs, FailureIsCapturedInHandle)
{
  using namespace AtlasNet;

  JobSystem system(JobSystem::Config{});

  auto handle =
      system.Submit([&](JobContext&) { throw std::runtime_error("boom"); },
                    JobOpts::Name{"FailureIsCapturedInHandle"});

  handle.wait();

  EXPECT_TRUE(handle.is_failed());

  EXPECT_THROW(handle.rethrow_if_failed(), std::runtime_error);

  system.Shutdown();
}

TEST(Jobs, CancelPendingJob)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::promise<void> blockerStarted;
  auto blockerStartedFuture = blockerStarted.get_future();

  std::promise<void> releaseBlocker;
  auto releaseBlockerFuture = releaseBlocker.get_future();

  std::atomic<bool> cancelledJobRan{false};

  auto blocker = system.Submit(
      [&](AtlasNet::JobContext&)
      {
        blockerStarted.set_value();
        releaseBlockerFuture.wait();
      },
      JobOpts::Name{"Blocker"}, JobOpts::TPriority<JobPriority::eHigh>{});

  EXPECT_EQ(blockerStartedFuture.wait_for(1s), std::future_status::ready);

  auto victim = system.Submit([&](JobContext&) { cancelledJobRan = true; },
                              JobOpts::Name{"Victim"},
                              JobOpts::TPriority<JobPriority::eLow>{});

  victim.cancel();

  releaseBlocker.set_value();
  blocker.wait();
  victim.wait();

  EXPECT_FALSE(cancelledJobRan.load());
  EXPECT_EQ(victim.state(), AtlasNet::JobState::eCancelled);

  system.Shutdown();
}
TEST(Jobs, FinishJobsBeforeQuitting)
{
  using namespace AtlasNet;
  JobSystem system(JobSystem::Config{});

  std::atomic<int> calls{0};
  constexpr int desiredRuns = 2000;
  for (int i = 0; i < desiredRuns; ++i)
  {
    system.Submit(
        [&](AtlasNet::JobContext& ctx)
        {
          ++calls;
        },
        JobOpts::Name(std::format("Job {}", i)),
        JobOpts::TPriority<JobPriority::eLow>{});
  }

  system.Shutdown();
  EXPECT_GE(calls.load(), desiredRuns );
}
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
