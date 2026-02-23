#pragma once

class SH_EntityAuthorityTracker;

// Writes tracker telemetry into AuthorityManifest format.
class SH_TelemetryPublisher
{
  public:
	void PublishFromTracker(const SH_EntityAuthorityTracker& tracker) const;
};
