#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

#include "audio/audio.hpp"
#include "fft/fft.hpp"
#include "window/window.hpp"

static constexpr int pow(const int base, const int power) {
    if (power == 0) return 1;

    int result = base;

    for (int i = 0; i < power - 1; i++) {
        result *= base;
    }

    return result;
}

constexpr int audio_buffer_size = pow(2, 13);
constexpr int sample_size = pow(2, 11);

float audio_buffer[audio_buffer_size]{};
int buffer_index = 0;

std::mutex mtx;
std::atomic<bool> active(true);

static void audio_thread_func() {
    while (active) {
        float samples[sample_size];

        int samples_retrived = fetch_audio_samples(samples, sample_size);

        //std::cout << samples_retrived << '\n';

        if (samples_retrived > 0) {
            std::lock_guard<std::mutex> lock(mtx);

            for (int i = 0; i < samples_retrived; i++) {
                audio_buffer[(buffer_index + i) & (audio_buffer_size - 1)] = samples[i];
            }

            buffer_index = (buffer_index + samples_retrived) & (audio_buffer_size - 1);

        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main() {
    bool init_result = init_audio();

    if (!init_result) {
        cleanup_audio();
        std::cerr << "Audio initialization failed.";
        return 0x1;
    }

    constexpr int width = 1500; constexpr int height = width * 9 / 16;
    init_result = init_window(width, height);

    if (!init_result) {
        cleanup_window();
        cleanup_audio();
        std::cerr << "Window initialization failed.";
        return 0x2;
    }

    std::thread audio_thread(audio_thread_func);
    
    while (active) {
        if (!update_window()) {
            active = false;
            break;
        }

        float local_buffer[audio_buffer_size];

        {
            std::lock_guard<std::mutex> lock(mtx);
            std::copy(std::begin(audio_buffer), std::end(audio_buffer), local_buffer);
        }

        render_frame(local_buffer, audio_buffer_size);

		std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }

    audio_thread.join();

    cleanup_window();
    cleanup_audio();
    return 0;
}
