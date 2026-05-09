import time       # Measures benchmark timing
import math       # Math functions like abs-style operations

# Initialization

SAMPLE_RATE = 48000
BUFFER_SIZE = 128
BUFFER_TIME_MS = (BUFFER_SIZE / SAMPLE_RATE) * 1000.0
DEADLINE_PERCENT = 50.0


class TestResult:
    def __init__(self):
        self.name = ""
        self.runs = 0
        self.total_ms = 0.0
        self.avg_ms = 0.0
        self.fastest_ms = 0.0
        self.slowest_ms = 0.0
        self.avg_deadline_percent = 0.0
        self.fastest_deadline_percent = 0.0
        self.slowest_deadline_percent = 0.0


# Report Functions

def print_test_report(result):
    print("----------------------------------------")
    print(f"Test: {result.name}")

    if result.name == "Gain":
        print("Work: multiply")
    elif result.name == "Overdrive":
        print("Work: multiply, abs, add, divide")
    elif result.name == "Looper":
        print("Work: buffer read/write, wraparound")
    elif result.name == "Noise Gate":
        print("Work: envelope, threshold, branch")
    elif result.name == "Compressor":
        print("Work: envelope, threshold, gain")
    elif result.name == "Wah":
        print("Work: resonant filter, sweep")
    elif result.name == "Chorus":
        print("Work: modulated delay, interpolation")
    elif result.name == "Flanger":
        print("Work: short delay, LFO, feedback")
    elif result.name == "Phaser":
        print("Work: all-pass filters, LFO")
    elif result.name == "Tape Delay":
        print("Work: delay buffer, feedback, filtering")
    elif result.name == "Spring Reverb":
        print("Work: delay network, feedback, filters")
    elif result.name == "Octave Up":
        print("Work: rectification / pitch effect")
    elif result.name == "Pitch Shift":
        print("Work: buffer reads, interpolation, pitch change")
    elif result.name == "Cab IR":
        print("Work: convolution, multiply-add")
    elif result.name == "Neural Amp":
        print("Work: model layers, nonlinear math")

    print(f"Runs: {result.runs}")
    print(f"Total ms: {result.total_ms}")
    print(
        f"Avg ms per buffer: {result.avg_ms} "
        f"({result.avg_deadline_percent}% of deadline)"
    )
    print(
        f"Fastest buffer ms: {result.fastest_ms} "
        f"({result.fastest_deadline_percent}% of deadline)"
    )
    print(
        f"Slowest buffer ms: {result.slowest_ms} "
        f"({result.slowest_deadline_percent}% of deadline)"
    )
    print(f"Buffer deadline ms: {BUFFER_TIME_MS}")

    if result.slowest_ms > BUFFER_TIME_MS:
        print("Audio status: BROKEN - at least one buffer missed the deadline")
    elif result.slowest_deadline_percent >= DEADLINE_PERCENT:
        print("Audio status: DANGER - very close to the deadline")
    else:
        print("Audio status: OK - buffers finished before the deadline")

    print("----------------------------------------\n")


def print_full_report(full_runtime_ms, test_count):
    full_avg_ms = full_runtime_ms / test_count
    full_deadline_percent = (full_avg_ms / BUFFER_TIME_MS) * 100.0

    print("========================================")
    print("Full chain report")
    print(f"Total selected runtime ms: {full_runtime_ms}")
    print(
        f"Avg ms per full buffer chain: {full_avg_ms} "
        f"({full_deadline_percent}% of deadline)"
    )
    print(f"Buffer deadline ms: {BUFFER_TIME_MS}")

    if full_avg_ms > BUFFER_TIME_MS:
        print("Full chain status: BROKEN - selected tests are too slow together")
    elif full_deadline_percent >= DEADLINE_PERCENT:
        print("Full chain status: DANGER - selected tests are getting close to the deadline")
    else:
        print("Full chain status: OK - selected tests fit inside the buffer deadline")

    print("========================================\n")


# Results Functions

def make_result(name, test_count, run_times):
    result = TestResult()
    result.name = name
    result.runs = test_count

    result.fastest_ms = run_times[0]
    result.slowest_ms = run_times[0]

    for time_ms in run_times:
        result.total_ms += time_ms

        if time_ms < result.fastest_ms:
            result.fastest_ms = time_ms

        if time_ms > result.slowest_ms:
            result.slowest_ms = time_ms

    result.avg_ms = result.total_ms / test_count

    result.avg_deadline_percent = (result.avg_ms / BUFFER_TIME_MS) * 100.0
    result.fastest_deadline_percent = (result.fastest_ms / BUFFER_TIME_MS) * 100.0
    result.slowest_deadline_percent = (result.slowest_ms / BUFFER_TIME_MS) * 100.0

    return result


def run_test_gain(test_count):
    input_buffer = [0.5] * BUFFER_SIZE
    output_buffer = [0.0] * BUFFER_SIZE
    run_times = []

    gain = 0.75

    for run in range(test_count):
        start_time = time.perf_counter()

        for i in range(BUFFER_SIZE):
            output_buffer[i] = input_buffer[i] * gain

        end_time = time.perf_counter()
        elapsed_ms = (end_time - start_time) * 1000.0

        run_times.append(elapsed_ms)

    result = make_result("Gain", test_count, run_times)
    print_test_report(result)
    return result


def run_test_overdrive(test_count):
    input_buffer = [0.5] * BUFFER_SIZE
    output_buffer = [0.0] * BUFFER_SIZE
    run_times = []

    gain = 2.5

    for run in range(test_count):
        start_time = time.perf_counter()

        for i in range(BUFFER_SIZE):
            x = input_buffer[i] * gain
            output_buffer[i] = x / (1.0 + abs(x))

        end_time = time.perf_counter()
        elapsed_ms = (end_time - start_time) * 1000.0

        run_times.append(elapsed_ms)

        deadline_percent = (elapsed_ms / BUFFER_TIME_MS) * 100.0

    result = make_result("Overdrive", test_count, run_times)
    print_test_report(result)
    return result


def run_test_looper(test_count):
    input_buffer = [0.5] * BUFFER_SIZE
    loop_buffer = [0.0] * SAMPLE_RATE
    output_buffer = [0.0] * BUFFER_SIZE
    run_times = []

    loop_position = 0

    for run in range(test_count):
        start_time = time.perf_counter()

        for i in range(BUFFER_SIZE):
            output_buffer[i] = loop_buffer[loop_position]
            loop_buffer[loop_position] = input_buffer[i]

            loop_position += 1

            if loop_position >= SAMPLE_RATE:
                loop_position = 0

        end_time = time.perf_counter()
        elapsed_ms = (end_time - start_time) * 1000.0

        run_times.append(elapsed_ms)

        deadline_percent = (elapsed_ms / BUFFER_TIME_MS) * 100.0

    result = make_result("Looper", test_count, run_times)
    print_test_report(result)
    return result


def run_test_flanger(test_count):
    input_buffer = [0.5] * BUFFER_SIZE
    output_buffer = [0.0] * BUFFER_SIZE
    delay_buffer = [0.0] * 512
    run_times = []

    write_position = 0
    feedback = 0.65

    for run in range(test_count):
        start_time = time.perf_counter()

        for i in range(BUFFER_SIZE):
            delay_samples = 8 + (run % 32)
            read_position = write_position - delay_samples

            if read_position < 0:
                read_position += len(delay_buffer)

            delayed = delay_buffer[read_position]

            output_buffer[i] = input_buffer[i] + delayed * 0.7
            delay_buffer[write_position] = input_buffer[i] + delayed * feedback

            write_position += 1

            if write_position >= len(delay_buffer):
                write_position = 0

        end_time = time.perf_counter()
        elapsed_ms = (end_time - start_time) * 1000.0

        run_times.append(elapsed_ms)

        deadline_percent = (elapsed_ms / BUFFER_TIME_MS) * 100.0

    result = make_result("Flanger", test_count, run_times)
    print_test_report(result)
    return result


def run_test_empty(test_count):
    result = TestResult()
    result.name = "Empty"
    result.runs = test_count

    print("----------------------------------------")
    print("Test: Empty")
    print("TODO: benchmark not added yet.")
    print("----------------------------------------\n")

    return result


# Main Menu

def print_main_menu():
    print("CackalackyCon 2026")
    print("Hack Your Guitar Tone - Benchmark (Python)\n")

    print("Audio settings:")
    print(f"Sample rate: {SAMPLE_RATE} Hz")
    print(f"Buffer size: {BUFFER_SIZE} frames")
    print(f"Time allowed per buffer: {BUFFER_TIME_MS} ms\n")

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
    print("\nB. Bail\n")


def main():
    while True:
        full_runtime_ms = 0.0

        print_main_menu()

        test_input = input("Test number: ")

        if test_input == "B" or test_input == "b":
            print("Bailing.")
            return

        test_count = int(input("Test count: "))

        print()

        if test_count <= 0:
            print("That was easy.\n")
            continue

        test_input = test_input.replace(",", " ")
        selected_tests = test_input.split()

        for selected_test in selected_tests:
            test_number = int(selected_test)

            if test_number == 1:
                result = run_test_gain(test_count)
            elif test_number == 2:
                result = run_test_overdrive(test_count)
            elif test_number == 3:
                result = run_test_looper(test_count)
            elif test_number == 8:
                result = run_test_flanger(test_count)
            elif test_number in [4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15]:
                result = run_test_empty(test_count)
            else:
                print("Wake up bro.\n")
                continue

            full_runtime_ms += result.total_ms

        print_full_report(full_runtime_ms, test_count)


if __name__ == "__main__":
    main()