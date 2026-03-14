#pragma once

#include <string>

struct StreamEntityLedgerEntry
{
	std::string EntityID;
	std::string ClientID;
	bool ISClient;
	float positionx, positiony, positionz;
	int WorldID;
	int BoundID;
};
