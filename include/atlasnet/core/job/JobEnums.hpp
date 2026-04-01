#pragma once

namespace AtlasNet
{
enum class JobPriority
{
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
enum class JobNotifyLevel
{
  eNone,
  eOnComplete,
  eOnStart
};

enum class JobState
{
  ePending, //It is pending
  eRunning, // A worker thread is currently executing this job
  eCompleted // The job has finished execution
};
} // namespace AtlasNet
