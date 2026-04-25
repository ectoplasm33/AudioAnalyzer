#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <iostream>
#include <thread>

#include "audio.hpp"

static IMMDeviceEnumerator* enumerator = nullptr;
static IMMDevice* device = nullptr;
static IAudioClient* audio_client = nullptr;
static IAudioCaptureClient* capture_client = nullptr;
static WAVEFORMATEX* wave_format = nullptr;
static int audio_format;
static int sample_rate;

int init_audio(int* _sample_rate) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    );
    if (FAILED(hr)) return 0x1;

    hr = enumerator->GetDefaultAudioEndpoint(
        eCapture,   // microphone
        eConsole,   // default device
        &device
    );

    if (FAILED(hr)) {
        // enumerate all active capture devices
        IMMDeviceCollection* collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (FAILED(hr)) return 0x2;

        unsigned int count = 0;
        collection->GetCount(&count);
        if (count == 0) {
            collection->Release();
            return 0x3;
        }

        hr = collection->Item(0, &device);
        if (FAILED(hr)) return false;
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client);
    if (FAILED(hr)) return 0x4;

    hr = audio_client->GetMixFormat(&wave_format); // get device format
    if (FAILED(hr)) return 0x5;

	int n_channels = wave_format->nChannels;
    sample_rate = wave_format->nSamplesPerSec;
    *_sample_rate = sample_rate;

    if (n_channels > 2) {
        // unsupported channel count
        CoTaskMemFree(wave_format);
        wave_format = nullptr;
        return 0x6;
	}

    if (wave_format->wFormatTag == WAVE_FORMAT_PCM && wave_format->wBitsPerSample == 16) {
        audio_format = 0x1;
    } else if (wave_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        audio_format = 0x2;
    } else if (wave_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)wave_format;

        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            if (ext->Format.wBitsPerSample == 16) {
                audio_format = 0x1;
            } else {
                audio_format = 0x3;
            }
            
        } else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            audio_format = 0x2;

        } else {
            // unsupported format
            CoTaskMemFree(wave_format);
            wave_format = nullptr;
            return 0x6;
        }
    } else {
        // unsupported format
        CoTaskMemFree(wave_format);
        wave_format = nullptr;
        return 0x6;
	}

	audio_format = audio_format << 2 | n_channels;

    // initialize audio client for capture
    hr = audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        10000000, // buffer duration in 100-ns units (~1 sec)
        0,
        wave_format,
        nullptr
    );
    if (FAILED(hr)) return 0x7;

    hr = audio_client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture_client);
    if (FAILED(hr)) return 0x8;

    hr = audio_client->Start(); // start capturing

	if (FAILED(hr)) return 0x9;

    return audio_format << 4;
}

int fetch_audio_samples(float* samples, const int buffer_size) {
    // Read available audio
    unsigned int packet_length = 0;
    HRESULT hr = capture_client->GetNextPacketSize(&packet_length);
    if (FAILED(hr)) return -1;
    if (packet_length == 0) return -2;

    int samples_read = 0;

    while (packet_length != 0 && samples_read < buffer_size) {
        unsigned char* data;
        UINT32 numFrames;
        DWORD flags;

        hr = capture_client->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
        if (FAILED(hr)) return -3;

        int frames_to_copy = std::min<UINT32>(numFrames, (UINT32)(buffer_size - samples_read));

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            for (UINT32 i = 0; i < frames_to_copy; i++) {
                samples[samples_read++] = 0.0f;
            }

            capture_client->ReleaseBuffer(numFrames);
            continue;
        }

		constexpr int format_PCM16_1 = (0x1 << 2) | 0x1;
		constexpr int format_PCM16_2 = (0x1 << 2) | 0x2;
		constexpr int format_IEEE_FLOAT_1 = (0x2 << 2) | 0x1;
		constexpr int format_IEEE_FLOAT_2 = (0x2 << 2) | 0x2;
        constexpr int format_PCM24_EXT_1 = (0x3 << 2) | 0x1;
        constexpr int format_PCM24_EXT_2 = (0x3 << 2) | 0x2;

        switch (audio_format) {
        case format_PCM16_1: {
            int16_t* pcm = (int16_t*)data;

            constexpr float scale = 1.0f / 32768.0f;

            for (UINT32 i = 0; i < frames_to_copy; i++) {
                samples[samples_read++] = (float)pcm[i] * scale;
            }

            break;
        }

        case format_IEEE_FLOAT_1: {
            float* pcm = (float*)data;

            for (UINT32 i = 0; i < frames_to_copy; i++) {
                samples[samples_read++] = pcm[i];
            }

            break;
        }

        case format_PCM24_EXT_1: {
            int32_t* pcm = (int32_t*)data;

            constexpr float scale = 1.0f / 8388608.0f;

            for (UINT32 i = 0; i < frames_to_copy; i++) {
                samples[samples_read++] = (float)pcm[i] * scale;
            }

            break;
        }

        case format_PCM16_2: {
            int16_t* pcm = (int16_t*)data;

            constexpr float scale = 1.0f / 65536.0f;

            UINT32 to_copy = frames_to_copy * 2;
            UINT32 i = 0;
            while (i < to_copy) {
                samples[samples_read++] = (float)(pcm[i++] + pcm[i++]) * scale;
            }

            break;
        }

        case format_IEEE_FLOAT_2: {
            float* pcm = (float*)data;

            UINT32 to_copy = frames_to_copy * 2;
            UINT32 i = 0;
            while (i < to_copy) {
                samples[samples_read++] = (pcm[i++] + pcm[i++]) * 0.5f;
            }

            break;
        }

        case format_PCM24_EXT_2: {
            int32_t* pcm = (int32_t*)data;

            constexpr float scale = 1.0f / 16777216.0f;

            UINT32 to_copy = frames_to_copy * 2;
            UINT32 i = 0;
            while (i < to_copy) {
                samples[samples_read++] = (float)(pcm[i++] + pcm[i++]) * scale;
            }

            break;
        }
        }

        hr = capture_client->ReleaseBuffer(numFrames);
        if (FAILED(hr)) return -5;

        hr = capture_client->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) return -6;
    }

    return samples_read;
}

void cleanup_audio() {
    if (capture_client) { capture_client->Release(); capture_client = nullptr; }
    if (audio_client) { audio_client->Release(); audio_client = nullptr; }
    if (device) { device->Release(); device = nullptr; }
    if (enumerator) { enumerator->Release(); enumerator = nullptr; }
    if (wave_format) { CoTaskMemFree(wave_format); wave_format = nullptr; }
    CoUninitialize();
}