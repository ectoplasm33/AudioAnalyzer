#include <iostream>
#include <thread>
#include <chrono>

#include "audio.hpp"
#include "fft.hpp"
#include "window.hpp"

static constexpr int pow(const int base, const int power) {
    if (power == 0) return 1;

    int result = base;

    for (int i = 0; i < power - 1; i++) {
        result *= base;
    }

    return result;
}

int main() {
    constexpr int audio_buffer_size = pow(2, 16);
    constexpr int sample_size = pow(2, 11);

    float audio_buffer[audio_buffer_size]{};
    int buffer_index = 0;

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

    bool active = true;
    while (active) {
        if (!update_window()) {
            break;
        }

        float samples[sample_size];

        int samples_retrived = fetch_audio_samples(samples, sample_size);

        std::cout << samples_retrived << '\n';

        if (samples_retrived > 0) {
            for (int i = 0; i < samples_retrived; i++) {
                audio_buffer[(buffer_index + i) & (audio_buffer_size - 1)] = samples[i];
            }

            buffer_index = (buffer_index + samples_retrived) & (audio_buffer_size - 1);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        render_frame(audio_buffer, audio_buffer_size);
    }

    cleanup_window();
    cleanup_audio();
    return 0;
}
