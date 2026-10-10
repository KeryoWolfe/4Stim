#pragma once

#include <string>
#include <string_view>

// Alignment (docs/ALIGNMENT.md): per scene and role, how far an actor is
// moved from the scene's spot (sideways, forward, up), turned, and scaled,
// on top of where the animation puts them. Set in game from the HUD's Align
// tab and saved to Data\F4SE\Plugins\4Stim\Alignment.json, so a fix made
// once applies every time that scene plays. The scene lock (main.cpp) holds
// each actor at the scene's spot plus their offset.
namespace Alignment
{
	struct Offset
	{
		float x = 0.0F;      // to the actor's right, game units
		float y = 0.0F;      // forward
		float z = 0.0F;      // up
		float rot = 0.0F;    // degrees, clockwise seen from above
		float scale = 1.0F;  // times the actor's own scale

		[[nodiscard]] bool IsZero() const { return x == 0.0F && y == 0.0F && z == 0.0F && rot == 0.0F && scale == 1.0F; }
	};

	// a_sceneID's offset for role a_role (all zero if none was set).
	// Case-insensitive. Any thread.
	Offset Get(std::string_view a_sceneID, std::size_t a_role);

	// Sets it and saves the file. Any thread.
	void Set(std::string_view a_sceneID, std::size_t a_role, const Offset& a_offset);

	// Re-reads the file (FourStim.ReloadScenes).
	void Reload();
}
