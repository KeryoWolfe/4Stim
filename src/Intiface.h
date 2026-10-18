#pragma once

#include <string>

// Intiface Central (docs/API.md), 4Stim's own toy plugin: connects to the
// Intiface Central app (or any Buttplug server) and registers each device it
// reports as a sex toy (SexToys.h), so toys work without another mod.
// Departure from OStim: OStim leaves device support to other plugins through
// its SexToyInterface; 4Stim has the same interface, and this built in.
//
// Speaks Buttplug protocol version 3 over a WebSocket (WinHTTP): devices with
// ScalarCmd actuators (vibrate, oscillate, constrict...) get the toy's
// strength; linear and rotating ones aren't driven yet. The connection runs
// on its own thread and retries while Intiface isn't running.
namespace Intiface
{
	struct Config
	{
		bool        enabled = false;                 // bIntiface
		std::string address = "ws://127.0.0.1:12345"; // sIntifaceAddress
		bool        scan = true;                     // bIntifaceScan: look for new devices on connecting
	};
	Config& Settings();

	// A 4Stim.ini / MCM setting of this module (key lowercase).
	bool ReadSetting(const std::string& a_key, const std::string& a_value);

	// Starts the connection thread (once, at kPostLoad); it connects while
	// bIntiface=1.
	void Start();
}
