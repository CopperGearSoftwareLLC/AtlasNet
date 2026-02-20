#pragma once
#include <sw/redis++/subscriber.h>

#include <iostream>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Debug/Log.hpp"
#include "Events/EventEnums.hpp"
// #include "Events/EventSubscriber.hpp"

#include "Events/EventRegistry.hpp"
#include "Events/IEvent.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"
class EventSystem : public Singleton<EventSystem>
{
	NetworkIdentity ID;
	Log logger = Log("EventSystem");
	std::jthread ConsumeThread;
	std::atomic_bool HasSubscription = false;
	std::atomic_bool EventSystemInit = false;

   public:
	// std::function<EventResult
	using EventCallFn = std::function<void(const IEvent& e)>;
	std::unordered_map<EventTypeID, std::vector<EventCallFn>>
		event_subscriptions;
	EventSystem() = default;
	sw::redis::Subscriber redisSubscriber = InternalDB::Get()->Subscriber();
	void Init(const NetworkIdentity& _ID)
	{
		ID = _ID;
		logger.Debug("Event System Init");

		redisSubscriber.on_message(
			[this](std::string channel, std::string msg)
			{
				logger.DebugFormatted("Event {} received", channel);
				const EventTypeID eventID =
					EventRegistry::Get().GetEventTypeID(channel);
				const auto& callbacks = event_subscriptions.at(eventID);

				ByteReader br(msg);
				std::unique_ptr<IEvent> event =
					EventRegistry::Get().CreateFromTypeID(eventID);
				event->Deserialize(br);
				for (const auto& f : callbacks)
				{
					f(*event);
				}
			});
		redisSubscriber.on_meta(
			[](sw::redis::Subscriber::MsgType type,
			   const std::optional<std::string>& channel, long long count)
			{
				std::cout << "Meta event type: " << static_cast<int>(type)
						  << ", channel: " << channel.value()
						  << ", subscription count: " << count << "\n";
			});

		ConsumeThread = std::jthread(
			[this](std::stop_token st)
			{
				// Wait until at least one subscription exists
				while (!HasSubscription.load(std::memory_order_acquire))
				{
					if (st.stop_requested())
						return;

					std::this_thread::sleep_for(std::chrono::milliseconds(20));
				}
				while (!st.stop_requested())
				{
					Update();
				}
			});
		EventSystemInit.store(true, std::memory_order_release);
	}

	template <typename T, typename F>
		requires std::is_base_of_v<IEvent, T> &&
				 std::is_invocable_r_v<void, F, const T&>
	void Subscribe(F&& f)
	{
		ASSERT(EventSystemInit.load(std::memory_order_acquire),
			   "Event System has not been initialized");

		const EventTypeID eventID = EventRegistry::Get().GetEventTypeID<T>();
		const std::string_view EventName =
			EventRegistry::Get().GetEventName<T>();

		redisSubscriber.subscribe(EventName);

		EventCallFn wrapper = [func =
								   std::forward<F>(f)](const IEvent& e) -> void
		{ return func(static_cast<const T&>(e)); };

		event_subscriptions[eventID].push_back(std::move(wrapper));
		logger.DebugFormatted("Subscribed to {}.", EventName);

		HasSubscription.store(true, std::memory_order_release);
	}
	template <typename T>
		requires std::is_base_of_v<IEvent, T>
	void Dispatch(const T& event)
	{
		ASSERT(EventSystemInit.load(std::memory_order_acquire),
			   "Event System has not been initialized");

		const std::string_view eventName =
			EventRegistry::Get().GetEventName<T>();

		ByteWriter bw;
		event.Serialize(bw);
		InternalDB::Get()->Publish(eventName, bw.as_string_view());
		logger.DebugFormatted("Event {} dispatched", eventName);
	}

   private:
	void Update() { redisSubscriber.consume(); }
};