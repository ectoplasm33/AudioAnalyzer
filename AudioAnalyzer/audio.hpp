#pragma once

bool init_audio();

int fetch_audio_samples(float* samples, int buffer_size);

void cleanup_audio();