#include "Intiface.h"

#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include "4Stim/PluginInterface.h"
#include "SexToys.h"

namespace Intiface
{
	namespace
	{
		using json = nlohmann::json;

		Config     g_config;
		std::mutex g_configLock;

		Config CurrentConfig()
		{
			std::scoped_lock lock(g_configLock);
			return g_config;
		}

		// One ScalarCmd feature of a device (Buttplug's actuator).
		struct Actuator
		{
			int         index = 0;
			std::string type;  // "Vibrate", "Oscillate", "Constrict"...
		};

		// A device as a sex toy. The toy calls (SexToys.cpp, detached
		// threads) only set what it should do; the connection thread sends it.
		class DeviceToy final : public FourStimAPI::SexToy
		{
		public:
			const char*   getID() override { return id.c_str(); }
			const char*   getName() override { return name.c_str(); }
			std::uint16_t getUpdateInterval() override { return 0; }  // the connection thread paces the device itself

			void turnOn() override
			{
				std::scoped_lock lock(stateLock);
				active = true;
			}
			void turnOff() override
			{
				std::scoped_lock lock(stateLock);
				active = false;
				ResetLocked();
			}
			void update(float a_baseline, float a_peak, std::int16_t a_peakInterval) override
			{
				std::scoped_lock lock(stateLock);
				baseline = std::clamp(a_baseline, 0.0F, 1.0F);
				// Called on a peak (with peaks on): a pulse, about a third of
				// the time between peaks long.
				if (a_peakInterval > 0 && a_peak >= 0.0F) {
					pulseLevel = std::clamp(a_peak, 0.0F, 1.0F);
					pulseLeft = std::clamp(static_cast<float>(a_peakInterval) / 1000.0F * 0.35F, 0.08F, 0.4F);
				}
			}
			void peak(float a_magnitude, float a_duration) override
			{
				std::scoped_lock lock(stateLock);
				overrideLevel = std::clamp(a_magnitude, 0.0F, 1.0F);
				overrideLeft = std::max(a_duration, 0.1F);
			}
			void stop() override
			{
				std::scoped_lock lock(stateLock);
				ResetLocked();
			}

			// The strength now, a_seconds after the last call.
			float Level(float a_seconds)
			{
				std::scoped_lock lock(stateLock);
				float level = 0.0F;
				if (overrideLeft > 0.0F) {
					level = overrideLevel;
				} else if (pulseLeft > 0.0F) {
					level = std::max(pulseLevel, baseline);
				} else {
					level = baseline;
				}
				overrideLeft -= a_seconds;
				pulseLeft -= a_seconds;
				return active || overrideLeft > -a_seconds ? level : 0.0F;
			}

			std::string           id;
			std::string           name;
			int                   deviceIndex = 0;
			std::vector<Actuator> actuators;
			bool                  present = false;
			float                 sent = 0.0F;  // the strength the device has

		private:
			void ResetLocked()
			{
				baseline = 0.0F;
				pulseLeft = 0.0F;
				overrideLeft = 0.0F;
			}

			std::mutex stateLock;
			bool       active = false;
			float      baseline = 0.0F;
			float      pulseLevel = 0.0F;
			float      pulseLeft = 0.0F;
			float      overrideLevel = 0.0F;
			float      overrideLeft = 0.0F;
		};

		// Toys are never deleted (SexToys holds their pointers): a device that
		// goes away is marked absent and comes back as the same toy.
		std::mutex                              g_toysLock;
		std::vector<std::unique_ptr<DeviceToy>> g_toys;

		class Listener final : public FourStimAPI::SexToyReloadListener
		{
		public:
			void reloadToys() override
			{
				std::scoped_lock lock(g_toysLock);
				for (const auto& toy : g_toys) {
					if (toy->present) {
						SexToys::Interface()->addToy(toy.get());
					}
				}
			}
		};
		Listener g_listener;

		// ---- The WebSocket (WinHTTP) ----
		class Socket
		{
		public:
			~Socket() { Close(); }

			bool Open(const std::string& a_address)
			{
				std::string rest = a_address;
				bool        secure = false;
				if (rest.starts_with("wss://")) {
					secure = true;
					rest = rest.substr(6);
				} else if (rest.starts_with("ws://")) {
					rest = rest.substr(5);
				}
				std::string path = "/";
				if (const auto slash = rest.find('/'); slash != std::string::npos) {
					path = rest.substr(slash);
					rest = rest.substr(0, slash);
				}
				INTERNET_PORT port = secure ? 443 : 80;
				if (const auto colon = rest.rfind(':'); colon != std::string::npos) {
					try {
						port = static_cast<INTERNET_PORT>(std::stoi(rest.substr(colon + 1)));
					} catch (...) {
						return false;
					}
					rest = rest.substr(0, colon);
				}
				const std::wstring host(rest.begin(), rest.end());
				const std::wstring wpath(path.begin(), path.end());

				session = WinHttpOpen(L"4Stim", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
				if (!session) {
					return false;
				}
				WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);
				connection = WinHttpConnect(session, host.c_str(), port, 0);
				if (!connection) {
					return false;
				}
				HINTERNET request = WinHttpOpenRequest(connection, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
					secure ? WINHTTP_FLAG_SECURE : 0);
				if (!request) {
					return false;
				}
				const bool upgraded = WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) &&
				                      WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) &&
				                      WinHttpReceiveResponse(request, nullptr);
				if (upgraded) {
					socket = WinHttpWebSocketCompleteUpgrade(request, 0);
				}
				WinHttpCloseHandle(request);
				if (!socket) {
					return false;
				}
				// Receives wait as long as they need (the reader thread).
				DWORD none = 0;
				WinHttpSetOption(socket, WINHTTP_OPTION_RECEIVE_TIMEOUT, &none, sizeof(none));
				return true;
			}

			bool Send(const std::string& a_text)
			{
				std::scoped_lock lock(sendLock);
				return socket && WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, const_cast<char*>(a_text.data()),
				                     static_cast<DWORD>(a_text.size())) == NO_ERROR;
			}

			// One whole message; false when the connection is gone.
			bool Receive(std::string& a_out)
			{
				a_out.clear();
				std::array<char, 4096> buffer{};
				while (socket) {
					DWORD                            read = 0;
					WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
					if (WinHttpWebSocketReceive(socket, buffer.data(), static_cast<DWORD>(buffer.size()), &read, &type) != NO_ERROR) {
						return false;
					}
					if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
						return false;
					}
					a_out.append(buffer.data(), read);
					if (a_out.size() > 1 << 20) {
						return false;
					}
					if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
						return true;
					}
				}
				return false;
			}

			// Ends the connection; a Receive waiting on it returns false.
			void Close()
			{
				std::scoped_lock lock(sendLock);
				if (socket) {
					// Shutdown sends the close without waiting for the answer (the
					// reader may be waiting in a Receive); closing the handle ends that.
					WinHttpWebSocketShutdown(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
					WinHttpCloseHandle(socket);
					socket = nullptr;
				}
				if (connection) {
					WinHttpCloseHandle(connection);
					connection = nullptr;
				}
				if (session) {
					WinHttpCloseHandle(session);
					session = nullptr;
				}
			}

		private:
			std::mutex sendLock;
			HINTERNET  session = nullptr;
			HINTERNET  connection = nullptr;
			HINTERNET  socket = nullptr;
		};

		std::atomic<std::uint32_t> g_nextID = 1;

		std::string Message(const char* a_type, json a_body = json::object())
		{
			a_body["Id"] = g_nextID++;
			return json::array({ json{ { a_type, std::move(a_body) } } }).dump();
		}

		// A device from DeviceList / DeviceAdded. Returns whether it's driven.
		bool AddDevice(const json& a_device)
		{
			const auto  name = a_device.value("DeviceName", std::string{ "Device" });
			const int   index = a_device.value("DeviceIndex", -1);
			std::vector<Actuator> actuators;
			if (const auto messages = a_device.find("DeviceMessages"); messages != a_device.end() && messages->is_object()) {
				if (const auto scalar = messages->find("ScalarCmd"); scalar != messages->end() && scalar->is_array()) {
					for (std::size_t i = 0; i < scalar->size(); ++i) {
						actuators.push_back({ static_cast<int>(i), (*scalar)[i].value("ActuatorType", std::string{ "Vibrate" }) });
					}
				}
			}
			if (index < 0) {
				return false;
			}
			if (actuators.empty()) {
				REX::INFO("Intiface: \"{}\" has no vibrate-type motors, not used (linear and rotating devices aren't driven yet)", name);
				return false;
			}
			std::scoped_lock lock(g_toysLock);
			const auto       id = "intiface:" + name;
			auto it = std::ranges::find_if(g_toys, [&](const auto& t) { return t->id == id && (!t->present || t->deviceIndex == index); });
			if (it == g_toys.end()) {
				g_toys.push_back(std::make_unique<DeviceToy>());
				it = std::prev(g_toys.end());
				(*it)->id = id;
				(*it)->name = name;
			}
			(*it)->deviceIndex = index;
			(*it)->actuators = std::move(actuators);
			(*it)->present = true;
			(*it)->sent = 0.0F;
			REX::INFO("Intiface: \"{}\" (device {}) ready, {} motor(s)", name, index, (*it)->actuators.size());
			return true;
		}

		void RemoveDevice(int a_index)
		{
			std::scoped_lock lock(g_toysLock);
			for (auto& toy : g_toys) {
				if (toy->present && toy->deviceIndex == a_index) {
					toy->present = false;
					REX::INFO("Intiface: \"{}\" disconnected", toy->name);
				}
			}
		}

		// A message from the server (the reader thread).
		void Handle(const std::string& a_text)
		{
			json messages;
			try {
				messages = json::parse(a_text);
			} catch (...) {
				return;
			}
			if (!messages.is_array()) {
				return;
			}
			bool changed = false;
			for (const auto& message : messages) {
				if (!message.is_object()) {
					continue;
				}
				for (const auto& [type, body] : message.items()) {
					if (type == "DeviceList") {
						if (const auto devices = body.find("Devices"); devices != body.end() && devices->is_array()) {
							for (const auto& device : *devices) {
								changed = AddDevice(device) || changed;
							}
						}
					} else if (type == "DeviceAdded") {
						changed = AddDevice(body) || changed;
					} else if (type == "DeviceRemoved") {
						RemoveDevice(body.value("DeviceIndex", -1));
						changed = true;
					} else if (type == "Error") {
						REX::WARN("Intiface: {}", body.value("ErrorMessage", std::string{ "error" }));
					}
				}
			}
			if (changed) {
				SexToys::Reload();  // the player's scene gets the devices there are now
			}
		}

		void SendLevel(Socket& a_socket, DeviceToy& a_toy, float a_level)
		{
			json scalars = json::array();
			for (const auto& actuator : a_toy.actuators) {
				scalars.push_back({ { "Index", actuator.index }, { "Scalar", a_level }, { "ActuatorType", actuator.type } });
			}
			a_socket.Send(Message("ScalarCmd", { { "DeviceIndex", a_toy.deviceIndex }, { "Scalars", std::move(scalars) } }));
			a_toy.sent = a_level;
		}

		// One connection, until it drops or the settings change.
		void RunConnection(const Config& a_config, bool& a_loggedFailure)
		{
			Socket socket;
			if (!socket.Open(a_config.address)) {
				if (!a_loggedFailure) {
					REX::INFO("Intiface: no server at {} (is Intiface Central running with its server started?); trying again every few seconds",
						a_config.address);
					a_loggedFailure = true;
				}
				return;
			}
			a_loggedFailure = false;
			// Handshake: Buttplug's RequestServerInfo, message version 3.
			std::string reply;
			if (!socket.Send(Message("RequestServerInfo", { { "ClientName", "4Stim" }, { "MessageVersion", 3 } })) || !socket.Receive(reply)) {
				REX::WARN("Intiface: {} didn't answer", a_config.address);
				return;
			}
			int maxPing = 0;
			try {
				const auto info = json::parse(reply).at(0);
				if (info.contains("Error")) {
					REX::WARN("Intiface: refused the connection: {}", info["Error"].value("ErrorMessage", std::string{}));
					return;
				}
				const auto& server = info.at("ServerInfo");
				maxPing = server.value("MaxPingTime", 0);
				REX::INFO("Intiface: connected to \"{}\" at {}", server.value("ServerName", std::string{ "Intiface" }), a_config.address);
			} catch (...) {
				REX::WARN("Intiface: {} isn't a Buttplug server", a_config.address);
				return;
			}

			std::atomic<bool> connected = true;
			std::thread       reader([&]() {
                std::string text;
                while (connected && socket.Receive(text)) {
                    Handle(text);
                }
                connected = false;
            });
			socket.Send(Message("RequestDeviceList"));
			if (a_config.scan) {
				socket.Send(Message("StartScanning"));
			}

			constexpr auto TICK = std::chrono::milliseconds(50);
			auto           lastPing = std::chrono::steady_clock::now();
			while (connected) {
				std::this_thread::sleep_for(TICK);
				const auto now = CurrentConfig();
				if (!now.enabled || now.address != a_config.address) {
					break;
				}
				{
					std::scoped_lock lock(g_toysLock);
					for (const auto& toy : g_toys) {
						if (!toy->present) {
							continue;
						}
						const float level = std::round(toy->Level(0.05F) * 100.0F) / 100.0F;
						if (std::abs(level - toy->sent) >= 0.01F) {
							SendLevel(socket, *toy, level);
						}
					}
				}
				if (maxPing > 0 && std::chrono::steady_clock::now() - lastPing > std::chrono::milliseconds(maxPing / 2)) {
					socket.Send(Message("Ping"));
					lastPing = std::chrono::steady_clock::now();
				}
			}
			socket.Send(Message("StopAllDevices"));
			connected = false;
			socket.Close();
			reader.join();
			{
				std::scoped_lock lock(g_toysLock);
				for (auto& toy : g_toys) {
					toy->present = false;
				}
			}
			SexToys::Reload();
			REX::INFO("Intiface: disconnected");
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	bool ReadSetting(const std::string& a_key, const std::string& a_value)
	{
		std::scoped_lock lock(g_configLock);
		if (a_key == "bintiface") {
			g_config.enabled = std::stoi(a_value) != 0;
		} else if (a_key == "sintifaceaddress") {
			g_config.address = a_value.empty() ? std::string{ "ws://127.0.0.1:12345" } : a_value;
		} else if (a_key == "bintifacescan") {
			g_config.scan = std::stoi(a_value) != 0;
		} else {
			return false;
		}
		return true;
	}

	void Start()
	{
		static bool started = false;
		if (started) {
			return;
		}
		started = true;
		SexToys::Interface()->registerReloadListener(&g_listener);
		std::thread([]() {
			bool loggedFailure = false;
			while (true) {
				const auto config = CurrentConfig();
				if (config.enabled) {
					RunConnection(config, loggedFailure);
					std::this_thread::sleep_for(std::chrono::seconds(5));
				} else {
					loggedFailure = false;
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
			}
		}).detach();
	}
}
