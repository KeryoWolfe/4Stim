#pragma once

// 4Stim: calling Papyrus from native code on 1.10.163 (docs/OLDGEN.md).
//
// The VM's DispatchStaticCall / DispatchMethodCall / SendEvent take their
// arguments as a BSTThreadScrapFunction, the game's copy of MSVC's
// std::function: 0x40 bytes with the callable at +0x38 on NG and AE, 0x20
// bytes with it at +0x18 on 1.10.163 (an older MSVC). These helpers once
// built the 1.10.163 layout themselves. The combined CommonLibF4 now does
// it inside those functions (msvc::with_native_function, since the
// asdasdduck merge), so converting here as well handed it an object it read
// as a std::function and crashed on 1.10.163 (a beta tester's save load, 19
// November 2026: FourUndress.UsePapyrusUndressing at kPostLoadGame). The
// helpers now just call CommonLibF4's functions; they stay so the callers
// don't change.
namespace Compat
{
	// IVirtualMachine::DispatchStaticCall(object, function, callback, args...).
	template <class... Args>
	bool DispatchStaticCall(RE::BSScript::IVirtualMachine* a_vm, const RE::BSFixedString& a_object, const RE::BSFixedString& a_function,
		const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>& a_callback, Args... a_args)
	{
		return a_vm->DispatchStaticCall(a_object, a_function, a_callback, a_args...);
	}

	// IVirtualMachine::DispatchMethodCall(handle, object, function, callback, args...).
	template <class... Args>
	bool DispatchMethodCall(RE::BSScript::IVirtualMachine* a_vm, std::uint64_t a_handle, const RE::BSFixedString& a_object, const RE::BSFixedString& a_function,
		const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>& a_callback, Args... a_args)
	{
		return a_vm->DispatchMethodCall(a_handle, a_object, a_function, a_callback, a_args...);
	}

	// IVirtualMachine::SendEvent(handle, event, args, filter (every
	// script), callback) with these arguments.
	template <class... Args>
	void SendEvent(RE::BSScript::IVirtualMachine* a_vm, std::uint64_t a_handle, const RE::BSFixedString& a_event,
		const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>& a_callback, Args... a_args)
	{
		auto pack = [&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
			a_out = RE::BSScript::detail::PackVariables(a_args...);
			return true;
		};
		auto everyone = [](const RE::BSTSmartPointer<RE::BSScript::Object>&) { return true; };
		a_vm->SendEvent(a_handle, a_event, pack, everyone, a_callback);
	}
}
