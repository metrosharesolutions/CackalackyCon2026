-- Console benchmark runner for Lua
-- No third-party tools.

-- Initialization

local SAMPLE_RATE = 48000
local BUFFER_SIZE = 128
local BUFFER_TIME_MS = (BUFFER_SIZE / SAMPLE_RATE) * 1000.0
local DEADLINE_PERCENT = 50.0

-- Report Functions

local function get_work_line(test_name)
	if test_name == "Gain" then
		return "multiply"
	elseif test_name == "Overdrive" then
		return "multiply, abs, add, divide"
	elseif test_name == "Looper" then
		return "buffer read/write, wraparound"
	elseif test_name == "Noise Gate" then
		return "envelope, threshold, branch"
	elseif test_name == "Compressor" then
		return "envelope, threshold, gain"
	elseif test_name == "Wah" then
		return "resonant filter, sweep"
	elseif test_name == "Chorus" then
		return "modulated delay, interpolation"
	elseif test_name == "Flanger" then
		return "short delay, LFO, feedback"
	elseif test_name == "Phaser" then
		return "all-pass filters, LFO"
	elseif test_name == "Tape Delay" then
		return "delay buffer, feedback, filtering"
	elseif test_name == "Spring Reverb" then
		return "delay network, feedback, filters"
	elseif test_name == "Octave Up" then
		return "rectification / pitch effect"
	elseif test_name == "Pitch Shift" then
		return "buffer reads, interpolation, pitch change"
	elseif test_name == "Cab IR" then
		return "convolution, multiply-add"
	elseif test_name == "Neural Amp" then
		return "model layers, nonlinear math"
	end

	return "unknown"
end

local function print_test_report(result)
	print("----------------------------------------")
	print("Test: " .. result.name)
	print("Work: " .. get_work_line(result.name))
	print("Runs: " .. result.runs)
	print("Total ms: " .. result.total_ms)
	print("Avg ms per buffer: " .. result.avg_ms .. " (" .. result.avg_deadline_percent .. "% of deadline)")
	print("Fastest buffer ms: " .. result.fastest_ms .. " (" .. result.fastest_deadline_percent .. "% of deadline)")
	print("Slowest buffer ms: " .. result.slowest_ms .. " (" .. result.slowest_deadline_percent .. "% of deadline)")
	print("Buffer deadline ms: " .. BUFFER_TIME_MS)

	if result.slowest_ms > BUFFER_TIME_MS then
		print("Audio status: BROKEN - at least one buffer missed the deadline")
	elseif result.slowest_deadline_percent >= DEADLINE_PERCENT then
		print("Audio status: DANGER - very close to the deadline")
	else
		print("Audio status: OK - buffers finished before the deadline")
	end

	print("----------------------------------------\n")
end

local function print_full_report(full_runtime_ms, test_count)
	local full_avg_ms = full_runtime_ms / test_count
	local full_deadline_percent = (full_avg_ms / BUFFER_TIME_MS) * 100.0

	print("========================================")
	print("Full chain report")
	print("Total selected runtime ms: " .. full_runtime_ms)
	print("Avg ms per full buffer chain: " .. full_avg_ms .. " (" .. full_deadline_percent .. "% of deadline)")
	print("Buffer deadline ms: " .. BUFFER_TIME_MS)

	if full_avg_ms > BUFFER_TIME_MS then
		print("Full chain status: BROKEN - selected tests are too slow together")
	elseif full_deadline_percent >= DEADLINE_PERCENT then
		print("Full chain status: DANGER - selected tests are getting close to the deadline")
	else
		print("Full chain status: OK - selected tests fit inside the buffer deadline")
	end

	print("========================================\n")
end

-- Results Functions

local function make_result(name, test_count, run_times)
	local result = {
		name = name,
		runs = test_count,
		total_ms = 0.0,
		avg_ms = 0.0,
		fastest_ms = run_times[1],
		slowest_ms = run_times[1],
		avg_deadline_percent = 0.0,
		fastest_deadline_percent = 0.0,
		slowest_deadline_percent = 0.0
	}

	for i = 1, #run_times do
		local time_ms = run_times[i]

		result.total_ms = result.total_ms + time_ms

		if time_ms < result.fastest_ms then
			result.fastest_ms = time_ms
		end

		if time_ms > result.slowest_ms then
			result.slowest_ms = time_ms
		end
	end

	result.avg_ms = result.total_ms / test_count

	result.avg_deadline_percent = (result.avg_ms / BUFFER_TIME_MS) * 100.0
	result.fastest_deadline_percent = (result.fastest_ms / BUFFER_TIME_MS) * 100.0
	result.slowest_deadline_percent = (result.slowest_ms / BUFFER_TIME_MS) * 100.0

	return result
end

local function run_test_gain(test_count)
	local input = {}
	local output = {}
	local run_times = {}
	local gain = 0.75

	for i = 1, BUFFER_SIZE do
		input[i] = 0.5
		output[i] = 0.0
	end

	for run = 1, test_count do
		local start_time = os.clock()

		for i = 1, BUFFER_SIZE do
			output[i] = input[i] * gain
		end

		local end_time = os.clock()
		local elapsed_ms = (end_time - start_time) * 1000.0

		run_times[#run_times + 1] = elapsed_ms
	end

	local result = make_result("Gain", test_count, run_times)
	print_test_report(result)
	return result
end

local function run_test_overdrive(test_count)
	local input = {}
	local output = {}
	local run_times = {}
	local gain = 2.5

	for i = 1, BUFFER_SIZE do
		input[i] = 0.5
		output[i] = 0.0
	end

	for run = 1, test_count do
		local start_time = os.clock()

		for i = 1, BUFFER_SIZE do
			local x = input[i] * gain
			output[i] = x / (1.0 + math.abs(x))
		end

		local end_time = os.clock()
		local elapsed_ms = (end_time - start_time) * 1000.0

		run_times[#run_times + 1] = elapsed_ms
	end

	local result = make_result("Overdrive", test_count, run_times)
	print_test_report(result)
	return result
end

local function run_test_looper(test_count)
	local input = {}
	local loop_buffer = {}
	local output = {}
	local run_times = {}
	local loop_position = 1

	for i = 1, BUFFER_SIZE do
		input[i] = 0.5
		output[i] = 0.0
	end

	for i = 1, SAMPLE_RATE do
		loop_buffer[i] = 0.0
	end

	for run = 1, test_count do
		local start_time = os.clock()

		for i = 1, BUFFER_SIZE do
			output[i] = loop_buffer[loop_position]
			loop_buffer[loop_position] = input[i]

			loop_position = loop_position + 1

			if loop_position > SAMPLE_RATE then
				loop_position = 1
			end
		end

		local end_time = os.clock()
		local elapsed_ms = (end_time - start_time) * 1000.0

		run_times[#run_times + 1] = elapsed_ms
	end

	local result = make_result("Looper", test_count, run_times)
	print_test_report(result)
	return result
end

local function run_test_flanger(test_count)
	local input = {}
	local output = {}
	local delay_buffer = {}
	local run_times = {}
	local write_position = 1
	local feedback = 0.65

	for i = 1, BUFFER_SIZE do
		input[i] = 0.5
		output[i] = 0.0
	end

	for i = 1, 512 do
		delay_buffer[i] = 0.0
	end

	for run = 1, test_count do
		local start_time = os.clock()

		for i = 1, BUFFER_SIZE do
			local delay_samples = 8 + ((run - 1) % 32)
			local read_position = write_position - delay_samples

			if read_position < 1 then
				read_position = read_position + #delay_buffer
			end

			local delayed = delay_buffer[read_position]

			output[i] = input[i] + delayed * 0.7
			delay_buffer[write_position] = input[i] + delayed * feedback

			write_position = write_position + 1

			if write_position > #delay_buffer then
				write_position = 1
			end
		end

		local end_time = os.clock()
		local elapsed_ms = (end_time - start_time) * 1000.0

		run_times[#run_times + 1] = elapsed_ms
	end

	local result = make_result("Flanger", test_count, run_times)
	print_test_report(result)
	return result
end

local function run_test_empty(test_count)
	local result = {
		name = "Empty",
		runs = test_count,
		total_ms = 0.0,
		avg_ms = 0.0,
		fastest_ms = 0.0,
		slowest_ms = 0.0,
		avg_deadline_percent = 0.0,
		fastest_deadline_percent = 0.0,
		slowest_deadline_percent = 0.0
	}

	print("----------------------------------------")
	print("Test: Empty")
	print("TODO: benchmark not added yet.")
	print("----------------------------------------\n")

	return result
end

-- Main Menu

local function print_main_menu()
	print("CackalackyCon 2026")
	print("Hack Your Guitar Tone - Benchmark (Lua)\n")

	print("Audio settings:")
	print("Sample rate: " .. SAMPLE_RATE .. " Hz")
	print("Buffer size: " .. BUFFER_SIZE .. " frames")
	print("Time allowed per buffer: " .. BUFFER_TIME_MS .. " ms\n")

	print("Current tests:")
	print("1. Gain")
	print("2. Overdrive")
	print("3. Looper")
	print("4. Noise Gate (not implemented)")
	print("5. Compressor (not implemented)")
	print("6. Wah (not implemented)")
	print("7. Chorus (not implemented)")
	print("8. Flanger")
	print("9. Phaser (not implemented)")
	print("10. Tape Delay (not implemented)")
	print("11. Spring Reverb (not implemented)")
	print("12. Octave Up (not implemented)")
	print("13. Pitch Shift (not implemented)")
	print("14. Cab IR (not implemented)")
	print("15. Neural Amp (not implemented)")
	print("\nC. C++ Mode")
	print("\nB. Bail\n")
end

while true do
	local full_runtime_ms = 0.0

	print_main_menu()

	io.write("Test number: ")
	local test_input = io.read()

	if test_input == "C" or test_input == "c" then
		os.execute("clear")
		return
	end

	if test_input == "B" or test_input == "b" then
		print("Bailing.")
		os.exit()
	end

	io.write("Test count: ")
	local test_count = tonumber(io.read())

	print()

	if test_count == nil or test_count <= 0 then
		print("That was easy.\n")
	else
		test_input = string.gsub(test_input, ",", " ")

		for selected_test in string.gmatch(test_input, "%S+") do
			local test_number = tonumber(selected_test)
			local result = nil

			if test_number == 1 then
				result = run_test_gain(test_count)
			elseif test_number == 2 then
				result = run_test_overdrive(test_count)
			elseif test_number == 3 then
				result = run_test_looper(test_count)
			elseif test_number == 8 then
				result = run_test_flanger(test_count)
			elseif test_number == 4 or test_number == 5 or test_number == 6 or test_number == 7
				or test_number == 9 or test_number == 10 or test_number == 11
				or test_number == 12 or test_number == 13 or test_number == 14
				or test_number == 15 then
				result = run_test_empty(test_count)
			else
				print("Wake up bro.\n")
			end

			if result ~= nil then
				full_runtime_ms = full_runtime_ms + result.total_ms
			end
		end

		print_full_report(full_runtime_ms, test_count)
	end
end