#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <iostream>
#include <thread>

#include "audio.hpp"

static IMMDeviceEnumerator* enumerator = nullptr;
static IMMDevice* device = nullptr;
static IAudioClient* audioClient = nullptr;
static IAudioCaptureClient* captureClient = nullptr;
static WAVEFORMATEX* waveFormat = nullptr;

bool init_audio() {
    HRESULT hr = CoInitialize(nullptr);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    );
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(
        eCapture,   // microphone
        eConsole,   // default device
        &device
    );

    if (FAILED(hr)) {
        // enumerate all active capture devices
        IMMDeviceCollection* collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (FAILED(hr)) return false;

        unsigned int count = 0;
        collection->GetCount(&count);
        if (count == 0) {
            collection->Release();
            return false;
        }

        hr = collection->Item(0, &device);
        if (FAILED(hr)) return false;
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
    if (FAILED(hr)) return false;

    hr = audioClient->GetMixFormat(&waveFormat); // get device format
    if (FAILED(hr)) return false;

    // initialize audio client for capture
    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        10000000, // buffer duration in 100-ns units (~1 sec)
        0,
        waveFormat,
        nullptr
    );
    if (FAILED(hr)) return false;

    hr = audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&captureClient);
    if (FAILED(hr)) return false;

    hr = audioClient->Start(); // start capturing

    return SUCCEEDED(hr);
}

int fetch_audio_samples(float* samples, int buffer_size) {
    // Read available audio
    unsigned int packetLength = 0;
    HRESULT hr = captureClient->GetNextPacketSize(&packetLength);
    if (FAILED(hr)) return -1;
    if (packetLength == 0) return -2;

    int samplesRead = 0;

    while (packetLength != 0 && samplesRead < buffer_size) {
        unsigned char* data;
        UINT32 numFrames;
        DWORD flags;

        hr = captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
        if (FAILED(hr)) return -3;

        // Convert PCM16 to float if needed
        int channels = waveFormat->nChannels;
        int framesToCopy = std::min<UINT32>(numFrames, (UINT32)((buffer_size - samplesRead) / channels));

        UINT32 num = framesToCopy * channels;

        if (waveFormat->wFormatTag == WAVE_FORMAT_PCM && waveFormat->wBitsPerSample == 16) {
            int16_t* pcm = (int16_t*)data;

            constexpr float scale = 1.0f / 32768.0f;

            for (UINT32 i = 0; i < num; i++) {    
                samples[samplesRead++] = pcm[i] * scale;
            }

        } else if (waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            float* pcm = (float*)data;

            for (UINT32 i = 0; i < num; i++) {
                samples[samplesRead++] = pcm[i];
            }

        } else if (waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)waveFormat;

            if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
                int16_t* pcm = (int16_t*)data;
                constexpr float scale = 1.0f / 32768.0f;

                for (UINT32 i = 0; i < num; i++) {
                    samples[samplesRead++] = pcm[i] * scale;
                }

            } else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
                float* pcm = (float*)data;

                for (UINT32 i = 0; i < num; i++) {
                    samples[samplesRead++] = pcm[i];
                }

            } else {
                captureClient->ReleaseBuffer(numFrames);
                return -40; // still unsupported
            }
        } else {
            // unsupported format
            captureClient->ReleaseBuffer(numFrames);
            return -4;
        }

        hr = captureClient->ReleaseBuffer(numFrames);
        if (FAILED(hr)) return -5;

        hr = captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) return -6;
    }

    return samplesRead;
}

void cleanup_audio() {
    if (captureClient) { captureClient->Release(); captureClient = nullptr; }
    if (audioClient) { audioClient->Release(); audioClient = nullptr; }
    if (device) { device->Release(); device = nullptr; }
    if (enumerator) { enumerator->Release(); enumerator = nullptr; }
    if (waveFormat) { CoTaskMemFree(waveFormat); waveFormat = nullptr; }
    CoUninitialize();
}