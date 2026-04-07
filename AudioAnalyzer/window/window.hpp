#pragma once

bool init_window(const int width, const int height);

bool update_window();

void render_frame(const float* fft_buffer, const float* audio_buffer, const int buffer_size);

void cleanup_window();