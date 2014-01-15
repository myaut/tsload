/*
 * chisquare.c
 *
 *  Created on: Nov 26, 2013
 *      Author: myaut
 */

#include <randgen.h>
#include <tstime.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>

/**
 * Chi-Square is a simplest test to check random generators/variators.
 *
 * See. Raj Jain "The Art of Computer Systems Performance Analysis", p. 460
 *
 * chi - value of chi distributions for N intervals and p confidense
 * 	     (default: p = 0.95, N = 20)
 * number - number of test runs
 *
 * To generate sequences you may use CDF's of target distribution.
 * I.e. to calculate them for normal distribution:
 * 	$ python
 * 	>>> import math
	>>> mean = 0.0
	>>> stddev = 3.0

	>>> F = lambda x: 0.5 * (1 +  math.erf((x - mean) / math.sqrt(2) / stddev))

	>>> m1 = -2.0		# (d_min)
	>>> step = 0.25		# (d_max - d_min) / N

	>>> L = [F((i + 1) * step + m1) - F(i * step + m1)  for i in range(20)]
	>>> ', '.join('%.4f' % x for x in L)

	>>> F(m1)					# d_below
	>>> 1.0 - F(m1 + step * 20) # d_above

 * For erlang distribution:
 *  >>> shape = 2
 *  >>> rate = 2.0
 *  >>> F = lambda x: (1.0 - sum(1 / math.factorial(n) * math.exp(-rate * x) * math.pow(rate * x, n) for n in range(shape - 1)))
 *
 * FIXME: Values below min and above max are ignored
 * */

#define N 20

float chi = 31.410;
int number = 10000;

randgen_t* rg;

typedef struct {
	float d_min;
	float d_max;

	float d_below;
	float d_above;

	float d[N];
} distribution_t;

static distribution_t uniform = {
	0.0, 1.0,
	0.0, 0.0,

	{ 0.05, 0.05, 0.05, 0.05, 0.05,
	  0.05, 0.05, 0.05, 0.05, 0.05,
	  0.05, 0.05, 0.05, 0.05, 0.05,
	  0.05, 0.05, 0.05, 0.05, 0.05 }
};

static distribution_t exponential_r1 = {
	0.0, 2.0,
	0.0, 0.1353,

	{ 0.0952, 0.0861, 0.0779, 0.0705, 0.0638,
	  0.0577, 0.0522, 0.0473, 0.0428, 0.0387,
	  0.0350, 0.0317, 0.0287, 0.0259, 0.0235,
	  0.0212, 0.0192, 0.0174, 0.0157, 0.0142  }
};

static distribution_t exponential_r025 = {
	0.0, 20.0,
	0.0, 0.0067,

	{ 0.2212, 0.1723, 0.1342, 0.1045, 0.0814,
	  0.0634, 0.0494, 0.0384, 0.0299, 0.0233,
	  0.0182, 0.0141, 0.0110, 0.0086, 0.0067,
	  0.0052, 0.0041, 0.0032, 0.0025, 0.0019  }
};

static distribution_t normal_m0_s2 = {
	-5.0, 5.0,
	0.0062, 0.0062,

	{ 0.0060, 0.0105, 0.0173, 0.0267, 0.0388,
	  0.0530, 0.0680, 0.0819, 0.0928, 0.0987,
	  0.0987, 0.0928, 0.0819, 0.0680, 0.0530,
	  0.0388, 0.0267, 0.0173, 0.0105, 0.0060 }
};

static distribution_t normal_m1_s05 = {
	-0.5, 2.7,
	0.0013, 0.003,

	{ 0.0023, 0.0055, 0.0115, 0.0220, 0.0380,
	  0.0593, 0.0836, 0.1063, 0.1223, 0.1270,
	  0.1192, 0.1011, 0.0774, 0.0536, 0.0335,
	  0.0189, 0.0097, 0.0045, 0.0019, 0.0007 }
};

static distribution_t erlang_l2_k2 = {
	0.0, 4.0,
	0.0, 0.003,

	{ 0.0616, 0.1297, 0.1462, 0.1377, 0.1189,
	  0.0976, 0.0774, 0.0599, 0.0455, 0.0341,
	  0.0253, 0.0186, 0.0135, 0.0098, 0.0071,
	  0.0051, 0.0036, 0.0026, 0.0018, 0.0013 }
};

double chisquare_error(const char* s, int o, double d) {
	double observed = (double) o;
	double expected =  d * number;
	double error = pow(observed - expected, 2.0) / expected;

	printf("%16s %8.0f %8.0f %12.7f\n", s, expected, observed, error);

	return error;
}

boolean_t chisquare_test(distribution_t* d, randvar_t* rv) {
	int o[N] = { 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,
			 	 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, };
	int i, n;
	int below = 0, above = 0;
	double r;
	double error, D = 0.0;
	double k = (double) N / (d->d_max - d->d_min);

	char index_str[16];

	for(i = 0; i < number; ++i) {
		r = rv_variate_double(rv);

		if(r < d->d_min) {
			++below;
			continue;
		}

		if(r > d->d_max) {
			++above;
			continue;
		}

		n = (int) floor(k * (r - d->d_min));
		++o[n];
	}

	puts("   EXPECTED OBSERVED   ERROR");

	chisquare_error("<", below, d->d_below);

	for(i = 0; i < N; ++i) {
		snprintf(index_str, 16, "(%.2f;%.2f)",
					((double) i / k) + d->d_min,
					((double) (i + 1) / k) + d->d_min);
		D += chisquare_error(index_str, o[i], d->d[i]);
	}

	chisquare_error(">", above, d->d_above);

	printf("   D=%4.8f\n", D);

	return D < chi;
}

void test_uniform(void) {
	randvar_t* rv_uniform = rv_create(&rv_uniform_class, rg);

	puts("Uniform test\n");

	assert(chisquare_test(&uniform, rv_uniform));
	rv_destroy(rv_uniform);
}

void test_exponential(distribution_t* d, double rate) {
	randvar_t* rv_exponential = rv_create(&rv_exponential_class, rg);
	rv_set_double(rv_exponential, "rate", rate);

	printf("Exponential test with rate=%f\n", rate);

	assert(chisquare_test(d, rv_exponential));
	rv_destroy(rv_exponential);
}

void test_normal(distribution_t* d, double mean, double stddev) {
	randvar_t* rv_normal = rv_create(&rv_normal_class, rg);
	rv_set_double(rv_normal, "mean", mean);
	rv_set_double(rv_normal, "stddev", stddev);

	printf("Normal test with mean=%f, stddev=%f\n", mean, stddev);

	assert(chisquare_test(d, rv_normal));
	rv_destroy(rv_normal);
}

void test_erlang(distribution_t* d, int shape, double rate) {
	randvar_t* rv_erlang = rv_create(&rv_erlang_class, rg);
	rv_set_int(rv_erlang, "shape", shape);
	rv_set_double(rv_erlang, "rate", rate);

	printf("Erlang test with shape=%d, rate=%f\n", shape, rate);

	assert(chisquare_test(d, rv_erlang));
	rv_destroy(rv_erlang);
}

int test_main(void) {
	rg = rg_create(&rg_libc, tm_get_clock());

	test_uniform();

	test_exponential(&exponential_r1, 1.0);
	test_exponential(&exponential_r025, 0.25);

	test_normal(&normal_m0_s2, 0.0, 2.0);
	test_normal(&normal_m1_s05, 1.0, 0.5);

	test_erlang(&erlang_l2_k2, 2, 2.0);

	rg_destroy(rg);

	return 0;
}
