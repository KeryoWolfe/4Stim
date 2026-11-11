#include "Core/Core.h"

namespace Core::Natives::FourModEvents
{
	// ---- Mod events (FourModEvents, ModEvents.h): SKSE's for Fallout 4 ----
	// The receiver is any script object (Papyrus ScriptObject): a form, or,
	// as SKSE's RegisterForModEvent allows, a quest alias or an active magic
	// effect. Registrations are kept by its VM handle.
	std::uint64_t HandleOf(const RE::BSTSmartPointer<RE::BSScript::Object>& a_receiver)
	{
		return a_receiver ? static_cast<std::uint64_t>(a_receiver->GetHandle()) : 0;
	}

	bool Register(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, std::string a_eventName, std::string a_callback)
	{
		return ModEvents::Register(HandleOf(a_receiver), a_eventName, a_callback);
	}

	void Unregister(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, std::string a_eventName)
	{
		ModEvents::Unregister(HandleOf(a_receiver), a_eventName);
	}

	void UnregisterAll(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver)
	{
		ModEvents::UnregisterAll(HandleOf(a_receiver));
	}

	void Send(std::monostate, std::string a_eventName, std::string a_strArg, float a_numArg, RE::TESForm* a_sender)
	{
		ModEvents::Send(a_eventName, a_strArg, a_numArg, a_sender);
	}
}
