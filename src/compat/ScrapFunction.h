#pragma once

#include <cstddef>
#include <functional>
#include <new>
#include <typeinfo>
#include <utility>

// 4Stim: calling Papyrus from native code on 1.10.163 (docs/OLDGEN.md).
//
// The VM's DispatchStaticCall / DispatchMethodCall / SendEvent take their
// arguments as a BSTThreadScrapFunction, which is the game's copy of MSVC's
// std::function. CommonLibF4 passes a std::function as built today: 0x40
// bytes, the callable's pointer at +0x38, as NG and AE were built. 1.10.163
// was built with an older MSVC whose std::function is 0x20 bytes, the pointer
// at +0x18 (CommonLibF4RD's msvc::function), so the game read garbage there
// and crashed in DispatchStaticCall (a tester's crash log). On 1.10.163 these
// helpers pass an object laid out the old way; elsewhere they call
// CommonLibF4's functions as before. The callable's virtual functions (copy,
// move, call, target type, delete, destructor, get) are in the same order in
// both.
namespace Compat
{
	// 1.10.163 (anything before next-gen's 1.10.980).
	inline bool OldFunctionLayout()
	{
		static const bool old = REX::FModule::GetExecutingModule().GetFileVersion() < REL::Version{ 1, 10, 980, 0 };
		return old;
	}

	template <class>
	class OldFunction;

	// The 1.10.163 std::function layout, as CommonLibF4RD's msvc::function.
	// The callable always lives on the heap (never in the 0x18 bytes of
	// local storage), so the game's copies and deletes never write into it.
	template <class R, class... Args>
	class OldFunction<R(Args...)>
	{
	public:
		template <class F>
		explicit OldFunction(F&& a_function) :
			_fn(new Impl<std::decay_t<F>>(std::forward<F>(a_function)))
		{}

		OldFunction(const OldFunction&) = delete;
		OldFunction& operator=(const OldFunction&) = delete;

		~OldFunction()
		{
			if (_fn) {
				_fn->DeleteThis(true);
			}
		}

	private:
		class Proxy
		{
		public:
			virtual Proxy*                CopyTo(void*) = 0;                 // 00
			virtual Proxy*                MoveTo(void*) = 0;                 // 01
			virtual R                     DoCall(Args&&...) = 0;             // 02
			virtual const std::type_info& TargetType() const = 0;            // 03
			virtual void                  DeleteThis(bool a_deallocate) = 0;  // 04
			virtual ~Proxy() = default;                                       // 05
			virtual const void*           Get() const = 0;                   // 06
		};

		template <class F>
		class Impl final : public Proxy
		{
		public:
			template <class G>
			explicit Impl(G&& a_function) :
				_function(std::forward<G>(a_function))
			{}

			Proxy*                CopyTo(void*) override { return new Impl(_function); }
			Proxy*                MoveTo(void*) override { return new Impl(std::move(_function)); }
			R                     DoCall(Args&&... a_args) override { return std::invoke(_function, std::forward<Args>(a_args)...); }
			const std::type_info& TargetType() const override { return typeid(F); }
			void                  DeleteThis(bool a_deallocate) override
			{
				if (a_deallocate) {
					delete this;
				} else {
					this->~Impl();
				}
			}
			const void* Get() const override { return std::addressof(_function); }

		private:
			F _function;
		};

		alignas(alignof(long double)) std::byte _storage[3 * sizeof(void*)]{};  // 00
		Proxy*                                   _fn{ nullptr };                 // 18
	};
	static_assert(sizeof(OldFunction<void()>) == 0x20);

	// a_function as the type the VM's functions take, laid out for 1.10.163.
	template <class R, class... Args>
	const RE::BSTThreadScrapFunction<R(Args...)>& AsGameFunction(const OldFunction<R(Args...)>& a_function)
	{
		return reinterpret_cast<const RE::BSTThreadScrapFunction<R(Args...)>&>(a_function);
	}

	using PackFunction = bool(RE::BSScrapArray<RE::BSScript::Variable>&);
	using FilterFunction = bool(const RE::BSTSmartPointer<RE::BSScript::Object>&);

	// IVirtualMachine::DispatchStaticCall(object, function, callback, args...).
	template <class... Args>
	bool DispatchStaticCall(RE::BSScript::IVirtualMachine* a_vm, const RE::BSFixedString& a_object, const RE::BSFixedString& a_function,
		const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>& a_callback, Args... a_args)
	{
		if (!OldFunctionLayout()) {
			return a_vm->DispatchStaticCall(a_object, a_function, a_callback, a_args...);
		}
		const OldFunction<PackFunction> pack([&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
			a_out = RE::BSScript::detail::PackVariables(a_args...);
			return true;
		});
		return a_vm->DispatchStaticCall(a_object, a_function, AsGameFunction(pack), a_callback);
	}

	// IVirtualMachine::DispatchMethodCall(handle, object, function, callback, args...).
	template <class... Args>
	bool DispatchMethodCall(RE::BSScript::IVirtualMachine* a_vm, std::uint64_t a_handle, const RE::BSFixedString& a_object, const RE::BSFixedString& a_function,
		const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>& a_callback, Args... a_args)
	{
		if (!OldFunctionLayout()) {
			return a_vm->DispatchMethodCall(a_handle, a_object, a_function, a_callback, a_args...);
		}
		const OldFunction<PackFunction> pack([&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
			a_out = RE::BSScript::detail::PackVariables(a_args...);
			return true;
		});
		return a_vm->DispatchMethodCall(a_handle, a_object, a_function, AsGameFunction(pack), a_callback);
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
		if (!OldFunctionLayout()) {
			a_vm->SendEvent(a_handle, a_event, pack, everyone, a_callback);
			return;
		}
		const OldFunction<PackFunction>   oldPack(pack);
		const OldFunction<FilterFunction> oldEveryone(everyone);
		a_vm->SendEvent(a_handle, a_event, AsGameFunction(oldPack), AsGameFunction(oldEveryone), a_callback);
	}
}
