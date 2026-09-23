// SPDX-License-Identifier: GPL-2.0
/*
 * Phase 0 model for laser-induced multi-bit faults.
 *
 * This deliberately uses a repetition code so the distance and correction
 * boundary are explicit. It is a measurement model, not a kernel pointer ABI.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PAYLOAD_BITS 8
#define CORRECT_BITS 2
#define COPIES (2 * CORRECT_BITS + 1)
#define CODE_BITS (PAYLOAD_BITS * COPIES)

static uint64_t encode(uint8_t payload)
{
	uint64_t codeword = 0;
	unsigned int bit;
	unsigned int copy;

	for (bit = 0; bit < PAYLOAD_BITS; ++bit)
		for (copy = 0; copy < COPIES; ++copy)
			codeword |= (uint64_t)((payload >> bit) & 1U)
				<< (bit * COPIES + copy);
	return codeword;
}

static uint8_t decode(uint64_t codeword)
{
	uint8_t payload = 0;
	unsigned int bit;
	unsigned int copy;
	unsigned int votes;

	for (bit = 0; bit < PAYLOAD_BITS; ++bit) {
		votes = 0;
		for (copy = 0; copy < COPIES; ++copy)
			votes += (codeword >> (bit * COPIES + copy)) & 1U;
		if (votes > COPIES / 2)
			payload |= (uint8_t)(1U << bit);
	}
	return payload;
}

static uint64_t error_mask(unsigned int first, unsigned int second,
			   unsigned int third, unsigned int count)
{
	uint64_t mask = UINT64_C(1) << first;

	if (count > 1)
		mask |= UINT64_C(1) << second;
	if (count > 2)
		mask |= UINT64_C(1) << third;
	return mask;
}

static void expect(unsigned int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(EXIT_FAILURE);
	}
}

static void test_clean_codewords(void)
{
	unsigned int payload;

	for (payload = 0; payload < 256; ++payload)
		expect(decode(encode((uint8_t)payload)) == payload,
		       "clean codeword did not round-trip");
}

static void test_corrects_up_to_t_bits(void)
{
	const uint8_t payload = 0xa5;
	const uint64_t original = encode(payload);
	unsigned int first;
	unsigned int second;
	uint64_t corrupted;

	for (first = 0; first < CODE_BITS; ++first) {
		corrupted = original ^ error_mask(first, 0, 0, 1);
		expect(decode(corrupted) == payload,
		       "single-bit error was not corrected");
		for (second = first + 1; second < CODE_BITS; ++second) {
			corrupted = original ^ error_mask(first, second, 0, 2);
			expect(decode(corrupted) == payload,
			       "two-bit error was not corrected");
		}
	}
}

static void test_three_bit_boundary(void)
{
	const uint8_t payload = 0xa5;
	const uint64_t original = encode(payload);
	unsigned int first;
	unsigned int second;
	unsigned int third;
	unsigned int miscorrected = 0;

	for (first = 0; first < CODE_BITS; ++first)
		for (second = first + 1; second < CODE_BITS; ++second)
			for (third = second + 1; third < CODE_BITS; ++third)
				if (decode(original ^ error_mask(first, second, third, 3)) !=
				    payload)
					++miscorrected;

	printf("payload_bits=%u copies=%u code_bits=%u distance=%u\n",
	       PAYLOAD_BITS, COPIES, CODE_BITS, COPIES);
	printf("guaranteed_correction_bits=%u three_bit_miscorrections=%u\n",
	       CORRECT_BITS, miscorrected);
	expect(miscorrected > 0,
	       "boundary test unexpectedly corrected every three-bit error");
}

static void test_burst_examples(void)
{
	const uint8_t payload = 0x5a;
	const uint64_t original = encode(payload);
	unsigned int start;

	for (start = 0; start + CORRECT_BITS <= CODE_BITS; ++start) {
		uint64_t mask = 0;
		unsigned int bit;

		for (bit = 0; bit < CORRECT_BITS; ++bit)
			mask |= UINT64_C(1) << (start + bit);
		expect(decode(original ^ mask) == payload,
		       "two-bit burst was not corrected");
	}
}

int main(void)
{
	test_clean_codewords();
	test_corrects_up_to_t_bits();
	test_burst_examples();
	test_three_bit_boundary();
	printf("PASS: laser ECC Phase 0 model\n");
	return EXIT_SUCCESS;
}
