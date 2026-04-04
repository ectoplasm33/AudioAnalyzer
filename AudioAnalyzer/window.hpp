#pragma once

bool init_window(int width, int height);

bool update_window();

void render_frame(float* audio_buffer, int buffer_size);

void cleanup_window();