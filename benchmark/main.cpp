#include <iostream>  // Console input/output: std::cout, std::cin
#include <sstream>   // Turns "1,2 3" into separate test numbers
#include <string>    // Stores text like test names and menu input
#include <chrono>    // Measures benchmark timing
#include <vector>    // Stores audio buffers and timing results
#include <cmath>     // Math functions like std::abs and std::sin
#include <limits>    // Clears leftover input after std::cin

extern "C" {
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include <lua5.4/lauxlib.h>
}

// Initialization

const int SAMPLE_RATE = 48000;
const int BUFFER_SIZE = 128;
const double BUFFER_TIME_MS = (static_cast<double>(BUFFER_SIZE) / SAMPLE_RATE) * 1000.0;
const double DEADLINE_PERCENT = 50.0;

struct TestResult
{
	std::string name;
	int runs = 0;
	double total_ms = 0.0;
	double avg_ms = 0.0;
	double fastest_ms = 0.0;
	double slowest_ms = 0.0;
	double avg_deadline_percent = 0.0;
	double fastest_deadline_percent = 0.0;
	double slowest_deadline_percent = 0.0;
};

// Report Functions

void print_test_report(const TestResult& result)
{
	std::cout << "----------------------------------------\n";
	std::cout << "Test: " << result.name << "\n";
	
	if (result.name == "Gain")
	{
		std::cout << "Work: multiply\n";
	}
	else if (result.name == "Overdrive")
	{
		std::cout << "Work: multiply, abs, add, divide\n";
	}
	else if (result.name == "Looper")
	{
		std::cout << "Work: buffer read/write, wraparound\n";
	}
	else if (result.name == "Noise Gate")
	{
		std::cout << "Work: envelope, threshold, branch\n";
	}
	else if (result.name == "Compressor")
	{
		std::cout << "Work: envelope, threshold, gain\n";
	}
	else if (result.name == "Wah")
	{
		std::cout << "Work: resonant filter, sweep\n";
	}
	else if (result.name == "Chorus")
	{
		std::cout << "Work: modulated delay, interpolation\n";
	}
	else if (result.name == "Flanger")
	{
		std::cout << "Work: short delay, LFO, feedback\n";
	}
	else if (result.name == "Phaser")
	{
		std::cout << "Work: all-pass filters, LFO\n";
	}
	else if (result.name == "Tape Delay")
	{
		std::cout << "Work: delay buffer, feedback, filtering\n";
	}
	else if (result.name == "Spring Reverb")
	{
		std::cout << "Work: delay network, feedback, filters\n";
	}
	else if (result.name == "Octave Up")
	{
		std::cout << "Work: rectification / pitch effect\n";
	}
	else if (result.name == "Pitch Shift")
	{
		std::cout << "Work: buffer reads, interpolation, pitch change\n";
	}
	else if (result.name == "Cab IR")
	{
		std::cout << "Work: convolution, multiply-add\n";
	}
	else if (result.name == "Neural Amp")
	{
		std::cout << "Work: model layers, nonlinear math\n";
	}

	std::cout << "Runs: " << result.runs << "\n";
	std::cout << "Total ms: " << result.total_ms << "\n";
	std::cout << "Avg ms per buffer: " << result.avg_ms
		<< " (" << result.avg_deadline_percent << "% of deadline)\n";
	std::cout << "Fastest buffer ms: " << result.fastest_ms
		<< " (" << result.fastest_deadline_percent << "% of deadline)\n";
	std::cout << "Slowest buffer ms: " << result.slowest_ms
		<< " (" << result.slowest_deadline_percent << "% of deadline)\n";
	std::cout << "Buffer deadline ms: " << BUFFER_TIME_MS << "\n";

	if (result.slowest_ms > BUFFER_TIME_MS)
	{
		std::cout << "Audio status: BROKEN - at least one buffer missed the deadline\n";
	}
	else if (result.slowest_deadline_percent >= DEADLINE_PERCENT)
	{
		std::cout << "Audio status: DANGER - very close to the deadline\n";
	}
	else
	{
		std::cout << "Audio status: OK - buffers finished before the deadline\n";
	}

	std::cout << "----------------------------------------\n\n";
}

void print_full_report(double full_runtime_ms, int test_count)
{
	double full_avg_ms = full_runtime_ms / test_count;
	double full_deadline_percent = (full_avg_ms / BUFFER_TIME_MS) * 100.0;

	std::cout << "========================================\n";
	std::cout << "Full chain report\n";
	std::cout << "Total selected runtime ms: " << full_runtime_ms << "\n";
	std::cout << "Avg ms per full buffer chain: " << full_avg_ms
		<< " (" << full_deadline_percent << "% of deadline)\n";
	std::cout << "Buffer deadline ms: " << BUFFER_TIME_MS << "\n";

	if (full_avg_ms > BUFFER_TIME_MS)
	{
		std::cout << "Full chain status: BROKEN - selected tests are too slow together\n";
	}
	else if (full_deadline_percent >= DEADLINE_PERCENT)
	{
		std::cout << "Full chain status: DANGER - selected tests are getting close to the deadline\n";
	}
	else
	{
		std::cout << "Full chain status: OK - selected tests fit inside the buffer deadline\n";
	}

	std::cout << "========================================\n\n";
}

// Results Functions

TestResult make_result(const std::string& name, int test_count, const std::vector<double>& run_times)
{
	TestResult result;
	result.name = name;
	result.runs = test_count;

	result.fastest_ms = run_times[0];
	result.slowest_ms = run_times[0];

	for (double time_ms : run_times)
	{
		result.total_ms += time_ms;

		if (time_ms < result.fastest_ms)
		{
			result.fastest_ms = time_ms;
		}

		if (time_ms > result.slowest_ms)
		{
			result.slowest_ms = time_ms;
		}
	}

	result.avg_ms = result.total_ms / test_count;

	result.avg_deadline_percent = (result.avg_ms / BUFFER_TIME_MS) * 100.0;
	result.fastest_deadline_percent = (result.fastest_ms / BUFFER_TIME_MS) * 100.0;
	result.slowest_deadline_percent = (result.slowest_ms / BUFFER_TIME_MS) * 100.0;

	return result;
}

TestResult run_test_gain(int test_count)
{
	std::vector<float> input(BUFFER_SIZE, 0.5f);
	std::vector<float> output(BUFFER_SIZE, 0.0f);
	std::vector<double> run_times;
	run_times.reserve(test_count);

	float gain = 0.75f;

	for (int run = 0; run < test_count; run++)
	{
		auto start_time = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < BUFFER_SIZE; i++)
		{
			output[i] = input[i] * gain;
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

		run_times.push_back(elapsed.count());
	}

	TestResult result = make_result("Gain", test_count, run_times);
	print_test_report(result);
	return result;
}

TestResult run_test_overdrive(int test_count)
{
	std::vector<float> input(BUFFER_SIZE, 0.5f);
	std::vector<float> output(BUFFER_SIZE, 0.0f);
	std::vector<double> run_times;
	run_times.reserve(test_count);

	float gain = 2.5f;

	for (int run = 0; run < test_count; run++)
	{
		auto start_time = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < BUFFER_SIZE; i++)
		{
			float x = input[i] * gain;
			output[i] = x / (1.0f + std::abs(x));
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

		run_times.push_back(elapsed.count());

		double deadline_percent = (elapsed.count() / BUFFER_TIME_MS) * 100.0;
	}

	TestResult result = make_result("Overdrive", test_count, run_times);
	print_test_report(result);
	return result;
}

TestResult run_test_looper(int test_count)
{
	std::vector<float> input(BUFFER_SIZE, 0.5f);
	std::vector<float> loop_buffer(SAMPLE_RATE, 0.0f);
	std::vector<float> output(BUFFER_SIZE, 0.0f);
	std::vector<double> run_times;
	run_times.reserve(test_count);

	int loop_position = 0;

	for (int run = 0; run < test_count; run++)
	{
		auto start_time = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < BUFFER_SIZE; i++)
		{
			output[i] = loop_buffer[loop_position];
			loop_buffer[loop_position] = input[i];

			loop_position++;

			if (loop_position >= SAMPLE_RATE)
			{
				loop_position = 0;
			}
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

		run_times.push_back(elapsed.count());

		double deadline_percent = (elapsed.count() / BUFFER_TIME_MS) * 100.0;
	}

	TestResult result = make_result("Looper", test_count, run_times);
	print_test_report(result);
	return result;
}

TestResult run_test_flanger(int test_count)
{
	std::vector<float> input(BUFFER_SIZE, 0.5f);
	std::vector<float> output(BUFFER_SIZE, 0.0f);
	std::vector<float> delay_buffer(512, 0.0f);
	std::vector<double> run_times;
	run_times.reserve(test_count);

	int write_position = 0;
	float feedback = 0.65f;

	for (int run = 0; run < test_count; run++)
	{
		auto start_time = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < BUFFER_SIZE; i++)
		{
			int delay_samples = 8 + (run % 32);
			int read_position = write_position - delay_samples;

			if (read_position < 0)
			{
				read_position += static_cast<int>(delay_buffer.size());
			}

			float delayed = delay_buffer[read_position];

			output[i] = input[i] + delayed * 0.7f;
			delay_buffer[write_position] = input[i] + delayed * feedback;

			write_position++;

			if (write_position >= static_cast<int>(delay_buffer.size()))
			{
				write_position = 0;
			}
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

		run_times.push_back(elapsed.count());

		double deadline_percent = (elapsed.count() / BUFFER_TIME_MS) * 100.0;
	}

	TestResult result = make_result("Flanger", test_count, run_times);
	print_test_report(result);
	return result;
}

TestResult run_test_empty(int test_count)
{
	TestResult result;
	result.name = "Empty";
	result.runs = test_count;

	std::cout << "----------------------------------------\n";
	std::cout << "Test: Empty\n";
	std::cout << "TODO: benchmark not added yet.\n";
	std::cout << "----------------------------------------\n\n";

	return result;
}

// Lua

void run_lua_mode()
{
	lua_State* lua = luaL_newstate();

	if (lua == nullptr)
	{
		std::cout << "Lua failed to start.\n\n";
		return;
	}

	luaL_openlibs(lua);

	int result = luaL_dofile(lua, "main.lua");

	if (result != LUA_OK)
	{
		std::cout << "Lua file failed: " << lua_tostring(lua, -1) << "\n\n";
		lua_close(lua);
		return;
	}

	lua_close(lua);
}

// Main Menu

void print_main_menu()
{
	std::cout << "CackalackyCon 2026\n";
	std::cout << "Hack Your Guitar Tone - Benchmark (C++)\n\n";

	std::cout << "Audio settings:\n";
	std::cout << "Sample rate: " << SAMPLE_RATE << " Hz\n";
	std::cout << "Buffer size: " << BUFFER_SIZE << " frames\n";
	std::cout << "Time allowed per buffer: " << BUFFER_TIME_MS << " ms\n\n";

	std::cout << "Current tests:\n";
	std::cout << "1. Gain\n";
	std::cout << "2. Overdrive\n";
	std::cout << "3. Looper\n";
	std::cout << "4. Noise Gate (not implemented)\n";
	std::cout << "5. Compressor (not implemented)\n";
	std::cout << "6. Wah (not implemented)\n";
	std::cout << "7. Chorus (not implemented)\n";
	std::cout << "8. Flanger\n";
	std::cout << "9. Phaser (not implemented)\n";
	std::cout << "10. Tape Delay (not implemented)\n";
	std::cout << "11. Spring Reverb (not implemented)\n";
	std::cout << "12. Octave Up (not implemented)\n";
	std::cout << "13. Pitch Shift (not implemented)\n";
	std::cout << "14. Cab IR (not implemented)\n";
	std::cout << "15. Neural Amp (not implemented)\n";
	std::cout << "\nL. Lua Mode\n";
	std::cout << "\nB. Bail\n\n";
}

int main()
{
	system("clear");

	while (true)
	{
		std::string test_input;
		int test_count = 0;
		double full_runtime_ms = 0.0;

		print_main_menu();

		std::cout << "Test number: ";
		std::getline(std::cin, test_input);

		if (test_input == "L" || test_input == "l")
		{
			system("clear");
			run_lua_mode();
			continue;
		}

		if (test_input == "B" || test_input == "b")
		{
			std::cout << "Bailing.\n";
			return 0;
		}

		std::cout << "Test count: ";
		std::cin >> test_count;
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		std::cout << "\n";

		if (test_count <= 0)
		{
			std::cout << "That was easy.\n\n";
			continue;
		}

		for (char& c : test_input)
		{
			if (c == ',')
			{
				c = ' ';
			}
		}

		std::stringstream selected_tests(test_input);
		int test_number = 0;

		while (selected_tests >> test_number)
		{
			TestResult result;

			switch (test_number)
			{
			case 1:
				result = run_test_gain(test_count);
				break;

			case 2:
				result = run_test_overdrive(test_count);
				break;

			case 3:
				result = run_test_looper(test_count);
				break;

			case 8:
				result = run_test_flanger(test_count);
				break;

			case 4:
			case 5:
			case 6:
			case 7:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				result = run_test_empty(test_count);
				break;

			default:
				std::cout << "Wake up bro.\n\n";
				continue;
			}

			full_runtime_ms += result.total_ms;
		}

		print_full_report(full_runtime_ms, test_count);
	}
}