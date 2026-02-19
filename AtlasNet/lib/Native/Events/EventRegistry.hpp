#pragma once

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <cstdint>
#include <memory>
#include <string_view>
#include <typeindex>
#include <unordered_map>

#include "Events/Events/IEvent.hpp"
#include "Global/Misc/Singleton.hpp"

class EventRegistry : public Singleton<EventRegistry>
{
   private:
	static constexpr uint32_t HashName(const std::string_view str)
	{
		const char* c = str.data();
		uint32_t hash = 2166136261u;
		while (*c)
		{
			hash ^= static_cast<uint8_t>(*c++);
			hash *= 16777619u;
		}
		return hash;
	}
	using FactoryFn = std::unique_ptr<IEvent> (*)();
	struct EventTypeEntry
	{
		FactoryFn factory;
		std::string Name;
		std::type_index type_index;
		EventTypeID ID;
	};

   public:
	struct EventsByTypeIndex
	{
	};
	struct EventsByTypeID
	{
	};
	boost::multi_index_container<
		EventTypeEntry,
		boost::multi_index::indexed_by<

			boost::multi_index::hashed_unique<
				boost::multi_index::tag<EventsByTypeID>,
				boost::multi_index::member<EventTypeEntry, EventTypeID,
										   &EventTypeEntry::ID>>,

			boost::multi_index::hashed_unique<
				boost::multi_index::tag<EventsByTypeIndex>,
				boost::multi_index::member<EventTypeEntry, std::type_index,
										   &EventTypeEntry::type_index>>>>
		EventFactories;
	// std::unordered_map<EventTypeID, EventTypeEntry> eventFactories;

	template <typename T>
	void RegisterEvent(const std::string_view EventName)
	{
		EventTypeID TypeID = HashName(EventName);
		EventTypeEntry entry{.type_index = typeid(T)};
		entry.ID = TypeID;
		entry.factory = []() -> std::unique_ptr<IEvent>
		{ return std::make_unique<T>(); };
		entry.Name = EventName;
		EventFactories.insert(entry);
	};
	template <typename T>
	EventTypeID GetEventTypeID()
	{
		return EventFactories.get<EventsByTypeID>().find(typeid(T))->ID;
	}
};

#define ATLASNET_REGISTER_EVENT(Type, Name)                  \
	static const bool Event_Type##_registered = []() -> bool \
	{                                                        \
		EventRegistry::Get().RegisterEvent<Type>(Name);      \
		return true;                                         \
	}()