#pragma once

#include <boost/container/flat_map.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <typeindex>

#include "Command/NetCommand.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"

class CommandRegistry : public Singleton<CommandRegistry>
{
	using FactoryFn = std::unique_ptr<INetCommand> (*)();
	Log logger = Log("CommandRegistry");
	boost::container::flat_map<CommandID, FactoryFn> factories;

   public:
	template <typename C>
		requires(std::is_base_of_v<INetCommand, C>)
	constexpr void RegisterCommand()
	{
		CommandID ID = C::GetCommandIDStatic();
		if (factories.contains(ID))
		{
			logger.ErrorFormatted("Command ID {} Already registered", ID);
			ASSERT(!factories.contains(ID), "TYPE ALREADY REGISTERED");
		}
		factories.insert(std::make_pair(
			ID, []() -> std::unique_ptr<INetCommand> { return std::make_unique<C>(); }));
		logger.DebugFormatted("Registered Command ID {} : {}", ID, C().GetCommandName());
	}

	std::unique_ptr<INetCommand> MakeFromID(CommandID ID)
	{
		if (!factories.contains(ID))
		{
			logger.ErrorFormatted("COMMAND ID {} NOT REGISTERED", ID);
			logger.ErrorFormatted("Existing Commands:");

			for (const auto& com : factories)
			{
				std::unique_ptr<INetCommand> c = com.second();
				logger.DebugFormatted("{} : {}", com.first, c->GetCommandName());
			}

			throw std::runtime_error("COMMAND ID NOT REGISTERED");
		}
		return factories.at(ID)();
	}
};

#define ATLASNET_REGISTER_COMMAND(Type)                 \
	inline const bool Type##_registered = []() -> bool  \
	{                                                   \
		CommandRegistry::Get().RegisterCommand<Type>(); \
		return true;                                    \
	}()
