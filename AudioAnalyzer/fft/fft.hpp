#pragma once

void init_fft(const int num_bits);

void fast_fourier_transform(const float* samples, float* out, const int size);

void fft_cleanup();