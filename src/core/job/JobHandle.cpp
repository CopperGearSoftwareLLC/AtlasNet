
#include "atlasnet/core/job/JobHandle.hpp"
#include "atlasnet/core/job/JobSystem.hpp"

void AtlasNet::JobHandle::on_complete(Job onCompleteJob) const
{
  if (is_completed())
  {
    JobSystem::Get().PushJob(onCompleteJob);
    return;
  }

  std::unique_lock lock(runtime->mutex);
  if (is_completed())
  {
    JobSystem::Get().PushJob(onCompleteJob);
    return;
  }
  runtime->cv.wait(lock,
                   [this]
                   {
                     return runtime->state.load(std::memory_order_acquire) ==
                            JobState::eCompleted;
                   });
  JobSystem::Get().PushJob(onCompleteJob);
}
