#pragma once
#include <type_traits>
#include <unordered_set>
#include "Events/EventEnums.hpp"
//#include "Events/EventSubscriber.hpp"

#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Events/IEvent.hpp"
class EventSystem : public Singleton<EventSystem>
{
	NetworkIdentity ID;

   public:
   //std::function<EventResult
   //using EventCallFn =EventResult (*)(const IEvent& );
   //std::unordered_set<EventTypeID,std::vector<EventCallFn>> event_subscriptions;
	EventSystem() = default;

	void Init(const NetworkIdentity& _ID) { ID = _ID; }
    
  
    template <typename T>
    requires std::is_base_of_v<IEvent, T>
    void Subscribe(std::function<EventResult(const T& e)>)
    {

    }
};