#include <iostream>

static int* reversed_indexes;
static float* real;
static float* imag;
static float** cos_angles; 
static float** sin_angles;
static int buffer_size_bits;

void init_fft(const int num_bits) {
	int size = 1 << num_bits;

	reversed_indexes = new int[size];
	real = (float*)_aligned_malloc(size * sizeof(float), 32);
	imag = (float*)_aligned_malloc(size * sizeof(float), 32);
	
	for (int i = 0; i < size; i++) {
		int n = i;
		int r = 0;

		for (int j = 0; j < num_bits; j++) {
			r = (r << 1) | (n & 1);
			n >>= 1;
		}

		reversed_indexes[i] = r;
	}

	cos_angles = new float*[num_bits];
	sin_angles = new float*[num_bits];

	for (int i = 0; i < num_bits; i++) {
		int half = 1 << i;

		float arc = -6.28318530718f / (float)(half << 1);

		cos_angles[i] = new float[half];
		sin_angles[i] = new float[half];

		for (int j = 0; j < half; j++) {
			float angle = arc * (float)j;

			cos_angles[i][j] = std::cosf(angle);
			sin_angles[i][j] = std::sinf(angle);
		}
	}

	buffer_size_bits = num_bits;
}

void fast_fourier_transform(const float* samples, float* out, const int size) {
	for (int i = 0; i < size; i++) {
		real[i] = samples[reversed_indexes[i]];
	}

	__m256 zero_vec = _mm256_setzero_ps();

	int i = 0;
	while (i < size) {
		_mm256_store_ps(&imag[i], zero_vec);

		i += 8;
	}

	__m256 neg_one_vec = _mm256_set1_ps(-1.0f);

	// first stage
	for (int i = 0; i < size; i += 8) {
		__m256 r_even = _mm256_load_ps(real + i);
		__m256 r_odd = _mm256_mul_ps(_mm256_movehdup_ps(r_even), neg_one_vec);
		r_even = _mm256_moveldup_ps(r_even);

		__m256 re = _mm256_addsub_ps(r_even, r_odd);

		_mm256_store_ps(real + i, re);

		r_even = _mm256_load_ps(imag + i);
		r_odd = _mm256_mul_ps(_mm256_movehdup_ps(r_even), neg_one_vec);
		r_even = _mm256_moveldup_ps(r_even);

		__m256 im = _mm256_addsub_ps(r_even, r_odd);

		_mm256_store_ps(imag + i, im);
	}
	
	for (int stage = 1; stage < 3; stage++) {
		int group_size = 2 << stage;
		int half = 1 << stage;

		float* cos = cos_angles[stage];
		float* sin = sin_angles[stage];

		for (int i = 0; i < size; i += group_size) {
			for (int j = 0; j < half; j++) {
				int a = i + j;
				int b = a + half;

				float b_real = real[b];
				float b_imag = imag[b];

				float c = cos[j];
				float s = sin[j];

				float tr = b_real * c - b_imag * s;
				float ti = b_imag * c + b_real * s;

				real[b] = real[a] - tr;
				imag[b] = imag[a] - ti;
				real[a] += tr;
				imag[a] += ti;
			}
		}
	}

	for (int stage = 3; stage < buffer_size_bits; stage += 1) {
		int group_size = 2 << stage;
		int half = 1 << stage;

		float* cos = cos_angles[stage];
		float* sin = sin_angles[stage];

		for (int i = 0; i < size; i += group_size) {
			for (int j = 0; j < half; j += 8) {
				float* real_ptr = real + i + j;
				float* imag_ptr = imag + i + j;

				__m256 ra = _mm256_load_ps(real_ptr);
				__m256 rb = _mm256_load_ps(real_ptr + half);

				__m256 ia = _mm256_load_ps(imag_ptr);
				__m256 ib = _mm256_load_ps(imag_ptr + half);

				__m256 c = _mm256_load_ps(cos + j);
				__m256 s = _mm256_load_ps(sin + j);

				__m256 tr = _mm256_fmsub_ps(rb, c, _mm256_mul_ps(ib, s));
				__m256 ti = _mm256_fmadd_ps(ib, c, _mm256_mul_ps(rb, s));

				_mm256_store_ps(real_ptr, _mm256_add_ps(ra, tr));
				_mm256_store_ps(imag_ptr, _mm256_add_ps(ia, ti));
				_mm256_store_ps(real_ptr + half, _mm256_sub_ps(ra, tr));
				_mm256_store_ps(imag_ptr + half, _mm256_sub_ps(ia, ti));
			}
		}
	}

	float scale = 75.0f / (float)size;

	__m256 scale_avx = _mm256_set1_ps(scale);

	for (int i = 0; i < size; i += 8) {
		__m256 re = _mm256_load_ps(real + i);

		__m256 im = _mm256_load_ps(imag + i);

		__m256 sum = _mm256_fmadd_ps(im, im, _mm256_mul_ps(re, re));

		__m256 mag = _mm256_sqrt_ps(sum);

		_mm256_store_ps(out + i, _mm256_mul_ps(mag, scale_avx));
	}
}

void fft_cleanup() {
	delete[] reversed_indexes;
	_aligned_free(real);
	_aligned_free(imag);

	for (int i = 0; i < buffer_size_bits; i++) {
		delete[] cos_angles[i];
		delete[] sin_angles[i];
	}

	delete[] cos_angles;
	delete[] sin_angles;
}