#pragma once

#include <cstdint>
#include <filesystem>

// 4Stim: what compat/IDDB.cpp loaded, for the check at load (main.cpp).
namespace Compat
{
	// 4Stim's own address table is in use (an older game version), not the
	// Address Library.
	bool UsingAddressTable();

	// The table's path, when it's in use.
	std::filesystem::path AddressTablePath();

	// The table has this ID (always true with the Address Library).
	bool HasAddress(std::uint64_t a_id);
}
