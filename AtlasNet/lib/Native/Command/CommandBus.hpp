#pragma once
#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <mutex>
#include <type_traits>

#include "Command/CommandRegistry.hpp"
#include "NetCommand.hpp"
template <typename TargetType, typename RecvHeader, typename SendHeader>
class ICommandBus
{
	std::vector<std::pair<TargetType, std::unique_ptr<INetCommand>>> commandBuffer;
	std::mutex DispatchingMutex;
	using CallbackFunc = std::function<void(const RecvHeader&, const INetCommand&)>;
	boost::container::flat_map<CommandID, std::vector<CallbackFunc>> subscriptions;

   private:
	using ErasedHandler = std::function<void(const RecvHeader&, const void*)>;

	virtual void implParseCommand(TargetType target, const INetCommand& command) = 0;
	virtual void implFlushCommands() = 0;

   public:
	virtual ~ICommandBus() = default;
	template <typename T>
		requires(std::is_base_of_v<INetCommand, T>)
	void ExecCallback(const T& c)
	{
		const std::vector<CallbackFunc>& Callbacks = subscriptions.at(T::GetCommandIDStatic());

		std::for_each(Callbacks.cbegin(), Callbacks.cend(), [c = &c](CallbackFunc f) { f(c); });
	}
	template <typename T>
	void Dispatch(const TargetType& target, const T& c)
	{
		std::unique_lock lock(DispatchingMutex);

		commandBuffer.push_back(std::make_pair(target, std::make_unique<T>(c)));
	}

	template <typename T, typename F>
		requires std::is_invocable_v<F, const RecvHeader&, const T&>
	void Subscribe(F&& f)
	{
		const CommandID cmdID = T::GetCommandIDStatic();

		CallbackFunc wrapper = [func = std::forward<F>(f)](const RecvHeader& h,
														   const INetCommand& c) -> void
		{
			auto derived = dynamic_cast<const T*>(&c);
			if (!derived)
			{
				// handle error, e.g., wrong type dispatched
				throw std::bad_cast();
			}
			return func(h, *derived);
		};
		subscriptions[cmdID].push_back(std::move(wrapper));
	};

	void Flush()
	{
		std::unique_lock lock(DispatchingMutex);
		std::for_each(commandBuffer.begin(), commandBuffer.end(),
					  [this](const std::pair<TargetType, std::unique_ptr<INetCommand>>& v)
					  { implParseCommand(v.first, *v.second); });
		commandBuffer.clear();
		implFlushCommands();
	}
};