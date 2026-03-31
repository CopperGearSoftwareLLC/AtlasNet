#pragma once

#include <string>
#include <vector>

class AuthorityTelemetry
{
  public:
	AuthorityTelemetry() = default;

	void GetAllTelemetry(std::vector<std::vector<std::string>>& outTelemetry);
};
