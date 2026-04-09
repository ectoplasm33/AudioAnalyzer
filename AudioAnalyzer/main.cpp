#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

#include "audio/audio.hpp"
#include "fft/fft.hpp"
#include "window/window.hpp"

constexpr int buffer_size_bits = 12;
constexpr int audio_buffer_size = 1 << buffer_size_bits;
constexpr int sample_size = 1 << 11;

float audio_buffer[audio_buffer_size]{};
int buffer_index = 0;

std::mutex mtx;
std::atomic<bool> active(true);
std::atomic<bool> new_data(false);

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

            new_data = true;

        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main() {
    int init_audio_result = init_audio();

    if ((init_audio_result & 0xf) != 0) {
        cleanup_audio();
        std::cerr << "Audio initialization failed with code: " << init_audio_result;
        return 0x1;
    }

	std::cout << "Audio data format: " << ((init_audio_result >> 6) == 1 ? "PCM16" : ((init_audio_result >> 6) == 2 ? "IEEE float" : "PCM24")) << "\nAudio channel format: " << (((init_audio_result >> 4) & 0b11) == 1 ? "Mono" : "Stereo") << '\n';

    constexpr int width = 1500; constexpr int height = width * 9 / 16;
    bool init_window_result = init_window(width, height);

    if (!init_window_result) {
        cleanup_window();
        cleanup_audio();
        std::cerr << "Window initialization failed.";
        return 0x2;
    }

    init_fft(buffer_size_bits);

    std::thread audio_thread(audio_thread_func);

    float local_buffer[audio_buffer_size];

    float fft_output[audio_buffer_size];

    constexpr float scale = 6.28318530718f / (float)(audio_buffer_size - 1);

    float hann_coefficients[audio_buffer_size];
    for (int i = 0; i < audio_buffer_size; i++) {
        hann_coefficients[i] = 0.5f * (1.0f - std::cosf((float)i * scale));
    }
    
    while (active) {
        if (!update_window()) {
            active = false;
            break;
        }

        if (new_data) {
            float hann[audio_buffer_size];

            {
                std::lock_guard<std::mutex> lock(mtx);

                int start = (buffer_index - audio_buffer_size) & (audio_buffer_size - 1);

                std::copy_n(std::begin(audio_buffer) + buffer_index, audio_buffer_size - buffer_index, std::begin(local_buffer));
                std::copy_n(std::begin(audio_buffer), buffer_index, std::begin(local_buffer) + (audio_buffer_size - buffer_index));

                for (int i = 0; i < audio_buffer_size; i++) {
                    hann[i] = local_buffer[i] * hann_coefficients[i];
                }

                new_data = false;
            }

            fast_fourier_transform(hann, fft_output, audio_buffer_size);

            render_frame(local_buffer, fft_output, audio_buffer_size);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    audio_thread.join();

    fft_cleanup();

    cleanup_window();
    cleanup_audio();
    return 0;
}
