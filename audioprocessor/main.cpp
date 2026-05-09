// main.cpp
// Simple ALSA + Lua stereo processor.
// Build: g++ main.cpp -o pedal_test -lasound -llua5.4
//
// C++ asks for effect name.
// Lua asks for its own parameters.
// C++ opens USB audio and calls Lua processFrame(left, right).

#include <alsa/asoundlib.h>

extern "C" {
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include <lua5.4/lauxlib.h>
}

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <csignal>

static bool running = true;

static void stopApp(int)
{
	running = false;
}

static float cleanSample(float x)
{
	if (!std::isfinite(x)) return 0.0f;
	if (x > 1.0f) return 1.0f;
	if (x < -1.0f) return -1.0f;
	return x;
}

// Find first USB ALSA card from /proc/asound/cards.
static std::string findFirstUsbDevice()
{
	std::ifstream file("/proc/asound/cards");
	std::string line;
	int card = -1;

	while (std::getline(file, line)) {
		size_t bracket = line.find('[');

		if (bracket != std::string::npos) {
			try {
				card = std::stoi(line.substr(0, bracket));
			}
			catch (...) {
				card = -1;
			}
		}

		if (card >= 0 && (line.find("USB") != std::string::npos || line.find("usb") != std::string::npos)) {
			return "plughw:" + std::to_string(card) + ",0";
		}
	}

	return "plughw:0,0";
}

// Open stereo float ALSA device with simple defaults.
static bool openPcm(snd_pcm_t** pcm, const std::string& device, snd_pcm_stream_t stream)
{
	unsigned int rate = 48000;
	snd_pcm_uframes_t period = 256;

	if (snd_pcm_open(pcm, device.c_str(), stream, 0) < 0) return false;

	snd_pcm_hw_params_t* hw;
	snd_pcm_hw_params_alloca(&hw);

	snd_pcm_hw_params_any(*pcm, hw);
	snd_pcm_hw_params_set_access(*pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(*pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
	snd_pcm_hw_params_set_channels(*pcm, hw, 2);
	snd_pcm_hw_params_set_rate_near(*pcm, hw, &rate, nullptr);
	snd_pcm_hw_params_set_period_size_near(*pcm, hw, &period, nullptr);

	if (snd_pcm_hw_params(*pcm, hw) < 0) return false;

	snd_pcm_prepare(*pcm);
	return true;
}

static std::string ask(const std::string& label)
{
	std::cout << label << ": ";
	std::string value;
	std::getline(std::cin, value);
	return value;
}

// Load effect_name.lua and call init().
static bool loadLua(lua_State** L, const std::string& luaFile)
{
	*L = luaL_newstate();
	luaL_openlibs(*L);

	if (luaL_dofile(*L, luaFile.c_str()) != LUA_OK) {
		std::cout << "Lua load error: " << lua_tostring(*L, -1) << "\n";
		return false;
	}

	lua_getglobal(*L, "init");

	if (lua_isfunction(*L, -1)) {
		if (lua_pcall(*L, 0, 0, 0) != LUA_OK) {
			std::cout << "Lua init error: " << lua_tostring(*L, -1) << "\n";
			return false;
		}
	}
	else {
		lua_pop(*L, 1);
	}

	return true;
}

// Send one stereo frame to Lua and get one stereo frame back.
static void processLua(lua_State* L, float inL, float inR, float& outL, float& outR)
{
	lua_getglobal(L, "processFrame");
	lua_pushnumber(L, inL);
	lua_pushnumber(L, inR);

	if (lua_pcall(L, 2, 2, 0) != LUA_OK) {
		std::cout << "Lua error: " << lua_tostring(L, -1) << "\n";
		lua_pop(L, 1);
		outL = inL;
		outR = inR;
		return;
	}

	outL = lua_isnumber(L, -2) ? (float)lua_tonumber(L, -2) : inL;
	outR = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : inR;

	lua_pop(L, 2);

	outL = cleanSample(outL);
	outR = cleanSample(outR);
}

int main()
{
	signal(SIGINT, stopApp);

	system("clear");
	
	std::string device = findFirstUsbDevice();
	std::cout << "Using audio device: " << device << "\n";

	std::string effectName = ask("Effect name");
	std::string luaFile = "effect_" + effectName + ".lua";

	std::cout << "Loading Lua file: " << luaFile << "\n";

	lua_State* L = nullptr;

	if (!loadLua(&L, luaFile)) {
		return 1;
	}

	snd_pcm_t* capture = nullptr;
	snd_pcm_t* playback = nullptr;

	if (!openPcm(&capture, device, SND_PCM_STREAM_CAPTURE)) {
		std::cout << "Could not open capture device.\n";
		return 1;
	}

	if (!openPcm(&playback, device, SND_PCM_STREAM_PLAYBACK)) {
		std::cout << "Could not open playback device.\n";
		return 1;
	}

	const int frames = 128;
	std::vector<float> input(frames * 2);
	std::vector<float> output(frames * 2);

	std::cout << "Running. Press Ctrl+C to stop.\n";

	while (running) {
		int readFrames = snd_pcm_readi(capture, input.data(), frames);

		if (readFrames < 0) {
			snd_pcm_recover(capture, readFrames, 1);
			continue;
		}

		for (int i = 0; i < readFrames; ++i) {
			float inL = input[i * 2 + 0];
			float inR = input[i * 2 + 1];
			float outL = inL;
			float outR = inR;

			processLua(L, inL, inR, outL, outR);

			output[i * 2 + 0] = outL;
			output[i * 2 + 1] = outR;
		}

		int written = snd_pcm_writei(playback, output.data(), readFrames);

		if (written < 0) {
			snd_pcm_recover(playback, written, 1);
		}
	}

	snd_pcm_close(capture);
	snd_pcm_close(playback);
	lua_close(L);

	return 0;
}