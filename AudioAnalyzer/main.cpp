#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>

#include "audio/audio.hpp"
#include "fft/fft.hpp"
#include "window/window.hpp"

constexpr int buffer_size_bits = 12;
constexpr int audio_buffer_size = 1 << buffer_size_bits;
constexpr int sample_size = 1 << 11;

float audio_buffer[audio_buffer_size]{};
int buffer_index = 0;

int sample_rate;

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
    int init_audio_result = init_audio(&sample_rate);

    float fsample_rate = (float)sample_rate;
    float inv_buffer_size = 1.0f / (float)audio_buffer_size;

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
   
    std::vector<float> peaks;
    
    while (active) {
        if (!update_window()) {
            active = false;
            break;
        }

        if (new_data) {
            float hann[audio_buffer_size];

            {
                std::lock_guard<std::mutex> lock(mtx);

                std::copy_n(std::begin(audio_buffer) + buffer_index, audio_buffer_size - buffer_index, std::begin(local_buffer));
                std::copy_n(std::begin(audio_buffer), buffer_index, std::begin(local_buffer) + (audio_buffer_size - buffer_index));

                for (int i = 0; i < audio_buffer_size; i++) {
                    hann[i] = local_buffer[i] * hann_coefficients[i];
                }

                new_data = false;
            }

            fast_fourier_transform(hann, fft_output, audio_buffer_size);

            peaks.clear();

            float max_amp = 0.0f;
            int max_amp_idx;

            float km1 = fft_output[0];
            float k = fft_output[1];
            float kp1 = fft_output[2];

            int end = audio_buffer_size >> 1;
            for (int i = 3; i < end; i++) {
                if (km1 < k && kp1 < k) {
                    peaks.push_back(i - 2);

                    if (k > max_amp) {
                        max_amp = k;
                        max_amp_idx = i - 2;
                    }

                    i++;
                }

                km1 = k;
                k = kp1;
                kp1 = fft_output[i];
            }

            float threshold = std::max(0.3333f * max_amp, 0.0001f);

            int valid = 0;

            for (int i : peaks) {
                float k = fft_output[i];

                if (k < threshold) continue;

                valid++;

                float km1 = fft_output[i - 1];
                float kp1 = fft_output[i + 1];

                float inv_total = 1.0f / (km1 + k + kp1);

                float f = (float)i;

                float freq = inv_total * (km1 * (f - 1.0f) + k * f + kp1 * (f + 1.0f)) * fsample_rate * inv_buffer_size;

                std::cout << freq << ' ';
            }
            if (valid > 0) std::cout << '\n';

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
