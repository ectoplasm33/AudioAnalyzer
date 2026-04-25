#pragma once

int init_audio(int* _sample_rate);

int fetch_audio_samples(float* samples, int buffer_size);

void cleanup_audio();