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
 * FIXME: Values below min and above max are ignored
 * */

#define N 20

float chi = 31.410;
int number = 5000;

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

boolean_t chisquare_test(distribution_t* d, randvar_t* rv) {
	int o[N] = { 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,
			 	 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, };
	int i, n;
	int below = 0, above = 0;
	double r;
	double observed, expected, error, D;
	double k = (double) N / (d->d_max - d->d_min);

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

	for(i = 0; i < N; ++i) {
		observed = (double) o[i];
		expected =  d->d[i] * number;
		error = pow(observed - expected, 2.0) / expected;
		D += error;

		printf("%2d %8.0f %8.0f %12.7f\n", i, expected, observed, error);
	}

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

int test_main(void) {
	rg = rg_create(&rg_libc, tm_get_clock());

	test_uniform();

	test_exponential(&exponential_r1, 1.0);
	test_exponential(&exponential_r025, 0.25);

	rg_destroy(rg);

	return 0;
}
