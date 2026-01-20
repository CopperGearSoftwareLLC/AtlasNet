#pragma once
#include "InterlinkIdentifier.hpp"
#include "Misc/Singleton.hpp"
class EventSystem : public Singleton<EventSystem>
{
	InterLinkIdentifier ID;

   public:
	EventSystem() = default;

	void Init(const InterLinkIdentifier& _ID) { 
        
        ID = _ID; 
    
    }
};