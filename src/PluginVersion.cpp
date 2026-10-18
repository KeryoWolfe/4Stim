#include <F4SE/F4SE.h>

// 4Stim's F4SE version data (newer F4SE reads it to decide whether to load
// the plugin), in place of CommonLibF4's generated one (xmake.lua's
// "4stim.plugin" rule): built on CommonLibF4RD's Runtime Database (and the
// Address Library on AE), 4Stim finds the game's code on any version, so it
// says so (signatures: address-independent) with both struct layouts, the
// next-gen one (1.10.980+, also used by 1.10.984) and AE's (1.11.137+), as
// CommonLibF4RD plugins do, instead of naming exact game versions. 1.10.163
// (old F4SE) asks F4SEPlugin_Query instead (main.cpp).
F4SE_PLUGIN_VERSION = []() noexcept {
	F4SE::PluginVersionData v{};
	v.PluginVersion({ FOURSTIM_VERSION_MAJOR, FOURSTIM_VERSION_MINOR, FOURSTIM_VERSION_PATCH, 0 });
	v.PluginName("4Stim");
	v.AuthorName("KeryoWolfe");
	v.addressIndependence = F4SE::PluginVersionData::kAddressIndependence_Signatures | F4SE::PluginVersionData::kAddressIndependence_AddressLibrary_1_11_137;
	v.structureIndependence = F4SE::PluginVersionData::kStructureIndependence_1_10_980Layout | F4SE::PluginVersionData::kStructureIndependence_1_11_137Layout;
	v.MinimumRequiredXSEVersion({ 0, 0, 0, 0 });
	return v;
}();
