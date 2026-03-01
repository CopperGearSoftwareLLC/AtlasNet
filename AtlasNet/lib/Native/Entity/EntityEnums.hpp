#pragma once

#include <boost/describe/enum.hpp>
enum class EntityTransferStage
{
	eNone,		// Nothing done yet
	ePrepare,	// A -> B notify to prepare to receive certain entities
	eReady,		// B -> A acknowledged
	eCommit,	// A -> B //Freeze simulation and remote calls, sends last snapshot to B
	eComplete,	// B -> A acknowledged, transfer complete

};
BOOST_DESCRIBE_ENUM(EntityTransferStage, eNone, ePrepare, eReady, eCommit, eComplete);
