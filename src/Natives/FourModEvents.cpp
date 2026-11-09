#include "Core/Core.h"

namespace Core::Natives::FourModEvents
{
	// ---- Mod events (FourModEvents, ModEvents.h): SKSE's for Fallout 4 ----
	bool Register(std::monostate, RE::TESForm* a_receiver, std::string a_eventName, std::string a_callback)
	{
		return ModEvents::Register(a_receiver, a_eventName, a_callback);
	}

	void Unregister(std::monostate, RE::TESForm* a_receiver, std::string a_eventName)
	{
		ModEvents::Unregister(a_receiver, a_eventName);
	}

	void UnregisterAll(std::monostate, RE::TESForm* a_receiver)
	{
		ModEvents::UnregisterAll(a_receiver);
	}

	void Send(std::monostate, std::string a_eventName, std::string a_strArg, float a_numArg, RE::TESForm* a_sender)
	{
		ModEvents::Send(a_eventName, a_strArg, a_numArg, a_sender);
	}
}
