#include "Core/Core.h"

namespace Core::Natives::FourJSON
{
	// FourJSON (OStim's OJSON): the end event's JSON.
	std::vector<RE::Actor*> GetActors(std::monostate, std::string a_json) { return ModEvents::JSONActors(a_json); }
	std::string             GetScene(std::monostate, std::string a_json) { return ModEvents::JSONScene(a_json); }
	std::vector<std::string> GetMetadata(std::monostate, std::string a_json) { return ModEvents::JSONMetadata(a_json); }
}
