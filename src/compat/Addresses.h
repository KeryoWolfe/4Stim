#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// 4Stim: what compat/IDDB.cpp loaded, for the check at load (main.cpp).
namespace Compat
{
	// 4Stim's own address table or the Runtime Database is in use (a game
	// version other than AE with its Address Library).
	bool UsingAddressTable();

	// The table's path, when it's in use.
	std::filesystem::path AddressTablePath();

	// The table has this ID (always true with the Address Library).
	bool HasAddress(std::uint64_t a_id);

	// The addresses come from CommonLibF4RD's Runtime Database
	// (f4rd-runtime.bin), with 4Stim's own table (if any) behind it.
	bool UsingRuntimeDatabase();

	// Where the Runtime Database was looked for (when it wasn't found).
	std::filesystem::path RuntimeDatabasePathLooked();
	// Each place it was looked for, and whether it was there (the log).
	std::vector<std::string> RuntimeDatabaseProbes();

	// Why HasAddress is false for a_id.
	std::string WhyMissing(std::uint64_t a_id);
}
