#include <iostream>

static struct complex {
	float real, imag;
};

static int* reversed_indexes;
static complex* twiddle_factors;

void init_fft(const int num_bits) {
	int size = 1 << num_bits;

	reversed_indexes = new int[size];
	twiddle_factors = new complex[size];
	
	for (int i = 0; i < size; i++) {
		int n = i;
		int r = 0;

		for (int j = 0; j < num_bits; j++) {
			r = (r << 1) | (n & 1);
			n >>= 1;
		}

		reversed_indexes[i] = r;
		twiddle_factors[i] = {0.0f, 0.0f};
	}
}

void fast_fourier_transform(const float* samples, float* out, const int size) {
	for (int i = 0; i < size; i++) {
		twiddle_factors[i] = {samples[reversed_indexes[i]], 0.0f};
	}

	for (int group_size = 2; group_size <= size; group_size <<= 1) {
		float arc = -6.28318530718f / (float)group_size;
		int half = group_size >> 1;

		for (int i = 0; i < size; i += group_size) {
			for (int j = 0; j < half; j++) {
				int a = i + j;
				int b = a + half;

				float angle = (float)j * arc;

				float cos = std::cosf(angle);
				float sin = std::sinf(angle);

				float tr = twiddle_factors[b].real * cos - twiddle_factors[b].imag * sin;
				float ti = twiddle_factors[b].imag * cos + twiddle_factors[b].real * sin;

				complex temp_a = twiddle_factors[a];

				twiddle_factors[a].real += tr;
				twiddle_factors[a].imag += ti;
				twiddle_factors[b].real = temp_a.real - tr;
				twiddle_factors[b].imag = temp_a.imag - ti;
			}
		}
	}

	for (int i = 0; i < size; i++) {
		float real = twiddle_factors[i].real;
		float imag = twiddle_factors[i].imag;

		out[i] = std::logf(std::sqrtf(real * real + imag * imag)*.02f + 1.0f);
	}
}

void fft_cleanup() {
	delete[] reversed_indexes;
	delete[] twiddle_factors;
}