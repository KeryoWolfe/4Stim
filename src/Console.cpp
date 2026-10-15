#include "Console.h"

namespace Console
{
	void Run(RE::TESObjectREFR* a_ref, std::string_view a_command)
	{
		const auto factories = RE::IFormFactory::GetFormFactories();
		const auto factory = factories[std::to_underlying(RE::ENUM_FORM_ID::kSCPT)];
		auto       script = factory ? static_cast<RE::Script*>(factory->DoCreate()) : nullptr;
		if (!script) {
			REX::WARN("Console: couldn't create a script for \"{}\"", a_command);
			return;
		}
		script->SetText(a_command);
		RE::ScriptCompiler compiler;
		script->CompileAndRun(&compiler, RE::COMPILER_NAME::kSystemWindow, a_ref);
		delete script;
	}
}
