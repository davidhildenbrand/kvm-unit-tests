/*
 * Tests vector instruction support
 *
 * Copyright 2018 IBM Corp.
 *
 * Authors:
 *    Janosch Frank <frankja@de.ibm.com>
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License version 2.
 */
#include <libcflat.h>
#include <asm/page.h>
#include <asm/facility.h>
#include <asm/interrupt.h>
#include <asm-generic/barrier.h>

static uint8_t pagebuf[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

/* Fills all vector registers with data from addr */
static inline void vlm_all(unsigned long *addr)
{
	asm volatile(" .machine z13\n"
		     " vlm 0, 15, %[a]\n"
		     : : [a]  "Q" (*addr)
		     :	"v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8",
			"v9", "v10", "v11", "v12", "v13", "v14", "v15");
	asm volatile(" .machine z13\n"
		     " vlm 16, 31, %[a]\n"
		     : : [a]  "Q" (*(addr+256/8))
		     :	"v16", "v17", "v18", "v19", "v20", "v21", "v22",
			"v23", "v24", "v25", "v26", "v27", "v28", "v29",
			"v30", "v31");
}

typedef union vector {
	__uint128_t v;
	uint64_t q[2];
	uint32_t d[4];
	uint16_t w[8];
	uint8_t h[16];
} vector;

static inline void vgef(vector *v1, vector *v2, void *addr, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl %%v1, %[v1]\n"
		     " vl %%v2, %[v2]\n"
		     " vgef %%v1, 0(%%v2, %[a]), %[m3]\n"
		     " vst %%v1, %[v1]\n"
		     " vst %%v2, %[v2]\n"
		     : [v1] "+Q" (v1->v),
		       [v2] "+Q" (v2->v)
		     : [a]  "a" (addr),
		       [m3] "i" (m3)
		     : "v1", "v2", "memory");
}

static void test_vgef(void)
{
	uint32_t data = 0x12345678ul;
	vector v1 = {
		.q[0] = -1ull,
		.q[1] = -1ull,
	};
	vector v2 = {
		.d[0] = -1,
		.d[1] = -1,
		.d[2] = 56789,
		.d[3] = -1,
	};

	report_prefix_push("vgef");

	/* load vector element number 2 with the data */
	vgef(&v1, &v2, (uint32_t *)((uint8_t *)&data - 56789), 2);
	report("element loaded", v1.d[2] == data);
	report("elements unmodified", v1.d[0] == -1 && v1.d[1] == -1 &&
				      v1.d[3] == -1);

	/* invalid element number */
	expect_pgm_int();
	vgef(&v1, &v2, 0, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void vgeg(vector *v1, vector *v2, void *addr, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl %%v1, %[v1]\n"
		     " vl %%v2, %[v2]\n"
		     " vgeg %%v1, 0(%%v2, %[a]), %[m3]\n"
		     " vst %%v1, %[v1]\n"
		     " vst %%v2, %[v2]\n"
		     : [v1] "+Q" (v1->v),
		       [v2] "+Q" (v2->v)
		     : [a]  "a" (addr),
		       [m3] "i" (m3)
		     : "v1", "v2", "memory");
}

static void test_vgeg(void)
{
	uint64_t data = 0x123456789abcdefull;
	vector v1 = {
		.q[0] = -1ull,
		.q[1] = -1ull,
	};
	vector v2 = {
		.q[0] = -1ull,
		.q[1] = 56789,
	};

	report_prefix_push("vgeg");

	/* load vector element number 1 with the data */
	vgeg(&v1, &v2, (uint64_t *)((uint8_t *)&data - 56789), 1);
	report("element loaded", v1.q[1] == data);
	report("elements unmodified", v1.q[0] == -1ull);

	/* invalid element number */
	expect_pgm_int();
	vgeg(&v1, &v2, 0, 2);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void vgbm(vector *v1, uint16_t i2)
{
	asm volatile(" .machine z13\n"
		     " vl %%v1, %[v1]\n"
		     " vgbm %%v1,%[i2]\n"
		     " vst %%v1, %[v1]\n"
		     : [v1] "+Q" (v1->v)
		     : [i2] "i" (i2)
		     : "v1", "memory");
}

static void test_vgbm(void)
{
	vector v1 = {};

	report_prefix_push("vgbm");

	vgbm(&v1, 0x00ff);
	report("i2 == 0x00ff", v1.q[0] == 0 && v1.q[1] == -1ull);

	vgbm(&v1, 0x0f00);
	report("i2 == 0x0f00", v1.q[0] == 0x00000000ffffffffull &&
			       v1.q[1] == 0);

	vgbm(&v1, 0x4218);
	report("i2 == 0x1818", v1.q[0] == 0x00ff00000000ff00ull &&
			       v1.q[1] == 0x000000ffff000000ull);

	vgbm(&v1, 0);
	report("i2 == 0", v1.v == 0);

	report_prefix_pop();
}

static inline void vlrep(vector *v1, uint64_t *data, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl %%v1, %[v1]\n"
		     " vlrep %%v1, %[a2], %[m3]\n"
		     " vst %%v1, %[v1]\n"
		     : [v1] "+Q" (v1->v)
		     : [a2] "Q" (*data),
		       [m3] "i" (m3)
		     : "v1", "memory");
}

static void test_vlrep(void)
{
	uint64_t data = 0x0123456789abcdefull;
	vector v1 = {};

	report_prefix_push("vrep");

	vlrep(&v1, &data, 0);
	report("8", v1.q[0] == 0x0101010101010101ull &&
		    v1.q[1] == 0x0101010101010101ull);
	vlrep(&v1, &data, 1);
	report("16", v1.q[0] == 0x0123012301230123ull &&
		     v1.q[1] == 0x0123012301230123ull);
	vlrep(&v1, &data, 2);
	report("32", v1.q[0] == 0x0123456701234567ull &&
		     v1.q[1] == 0x0123456701234567ull);
	vlrep(&v1, &data, 3);
	report("64", v1.q[0] == 0x0123456789abcdefull &&
		     v1.q[1] == 0x0123456789abcdefull);

	expect_pgm_int();
	vlrep(&v1, &data, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void vgm(vector *v1, uint8_t i2, uint8_t i3, uint8_t m4)
{
	asm volatile(" .machine z13\n"
		     " vl %%v1, %[v1]\n"
		     " vgm %%v1,%[i2],%[i3],%[m4]\n"
		     " vst %%v1, %[v1]\n"
		     : [v1]  "+Q" (v1->v)
		     : [i2]  "i" (i2),
		       [i3]  "i" (i3),
		       [m4]  "i" (m4)
		     : "v1", "memory");
}

static void test_vgm(void)
{
	vector v1 = {};

	report_prefix_push("vgm");

	vgm(&v1, 62, 62, 3);
	report("single bit 64", v1.q[0] == 0x2 && v1.q[1] == 0x2);

	vgm(&v1, 30, 30, 2);
	report("single bit 32", v1.q[0] == 0x0000000200000002ull &&
			        v1.q[1] == 0x0000000200000002ull);

	vgm(&v1, 14, 14, 1);
	report("single bit 16", v1.q[0] == 0x0002000200020002ull &&
			        v1.q[1] == 0x0002000200020002ull);

	vgm(&v1, 6, 6, 0);
	report("single bit 8", v1.q[0] == 0x0202020202020202ull &&
			       v1.q[1] == 0x0202020202020202ull);

	vgm(&v1, 7, 0, 0);
	report("wrapping", v1.q[0] == 0x8181818181818181ull &&
			   v1.q[1] == 0x8181818181818181ull);

	vgm(&v1, 60, 63, 0);
	report("unused bits", v1.q[0] == 0x0f0f0f0f0f0f0f0full &&
			      v1.q[1] == 0x0f0f0f0f0f0f0f0full);

	/* invalid element size */
	expect_pgm_int();
	vgm(&v1,0, 0, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void vlgv(uint64_t *r1, vector *v3, void *addr, uint8_t m4)
{
	asm volatile(" .machine z13\n"
		     " vl %%v3, %[v3]\n"
		     " vlgv %[r1], %%v3, 0(%[a2]), %[m4]\n"
		     " vst %%v3, %[v3]\n"
		     : [r1] "+d" (*r1),
		       [v3] "+Q" (v3->v)
		     : [a2] "d" (addr),
		       [m4] "i" (m4)
		     : "v3", "memory");
}

static void test_vlgv(void)
{
	vector v3 = {
		.q[0] = 0x0011223344556677ull,
		.q[1] = 0x8899aabbccddeeffull,
	};
	uint64_t reg = 0;

	report_prefix_push("vlgv");

	/* Directly set all ignored bits to 1 */
	vlgv(&reg, &v3, (void *)(7 | ~0xf), 0);
	report("8", reg == 0x77);
	vlgv(&reg, &v3, (void *)(4 | ~0x7), 1);
	report("16", reg == 0x8899);
	vlgv(&reg, &v3, (void *)(3 | ~0x3), 2 );
	report("32", reg == 0xccddeeff);
	vlgv(&reg, &v3, (void *)(1 | ~0x1), 3);
	report("64", reg == 0x8899aabbccddeeffull);

	expect_pgm_int();
	vlgv(&reg, &v3, NULL, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void va(vector *v1, vector *v2, vector *v3, uint8_t m4)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " va 1, 2, 3, %[m4]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v)
		     : [m4]  "i" (m4)
		     : "v1", "v2", "v3");
}

static void test_va(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0011223344556677ull, /* 0 - 7 */
		.q[1] = 0x8899aabbccddeeffull, /* 8 - 15 */
	};
	vector v3 = {
		.q[0] = 0x7766554433221100ull, /* 16 - 23 */
		.q[1] = 0xffeeddccbbaa9988ull, /* 24 - 31 */
	};

	report_prefix_push("va");

	va(&v1, &v2, &v3, 0);
	report("8", v1.q[0] == 0x7777777777777777ull &&
		    v1.q[1] == 0x8787878787878787ull);
	va(&v1, &v2, &v3, 1);
	report("16", v1.q[0] == 0x7777777777777777ull &&
		     v1.q[1] == 0x8887888788878887ull);
	va(&v1, &v2, &v3, 2);
	report("32", v1.q[0] == 0x7777777777777777ull &&
		     v1.q[1] == 0x8888888788888887ull);
	va(&v1, &v2, &v3, 3);
	report("64", v1.q[0] == 0x7777777777777777ull &&
		     v1.q[1] == 0x8888888888888887ull);
	va(&v1, &v2, &v3, 4);
	report("128", v1.q[0] == 0x7777777777777778ull &&
		      v1.q[1] == 0x8888888888888887ull);

	/* reserved ES */
	expect_pgm_int();
	va(&v1, &v2, &v3, 5);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static void test_vgfma(void)
{
	static struct prm {
		__uint128_t a,b,c,d;
	} prm __attribute__((aligned(16)));

	prm.a = 0x123456789abcdef0ul;
	prm.b = 0x76543210fedcba98ul;
	prm.c = 0x2029834098234234ul;
	prm.d = 0;

	asm volatile(" .machine z13\n"
		     " vl 0, %[v1]\n"
		     " vl 1, %[v2]\n"
		     " vl 2, %[v3]\n"
		     " vl 3, %[v4]\n"
		     " vgfma 3, 0, 1, 2, 3\n"
		     " vst 3, %[v4]\n"
		     : [v4]  "=Q" (prm.d)
		     : [v1]  "Q" (prm.a), [v2]  "Q" (prm.b), [v3]  "Q" (prm.c)
		     : "v0", "v1", "v2", "v3");
	report("vgfma", (uint64_t)(prm.d >> 64) == 0x781860285030484ull &&
			(uint64_t)prm.d == 0x23ac0146192442b4ul);
}

static inline void vlr(vector *v1, vector *v2)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vlr 1, 2\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v)
		     : : "v1", "v2");
}

static void test_vlr(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0011223344556677ull,
		.q[1] = 0x8899aabbccddeeffull,
	};

	vlr(&v1, &v2);
	report("vlr", v1.q[0] == v2.q[0] && v1.q[1] == v2.q[1]);

	report_prefix_pop();
}

static inline void vperm(vector *v1, vector *v2, vector *v3, vector *v4)
{
	asm volatile(" .machine z13\n"
		     " vl 0, %[v1]\n"
		     " vl 1, %[v2]\n"
		     " vl 2, %[v3]\n"
		     " vl 3, %[v4]\n"
		     " vperm 0, 1, 2, 3\n"
		     " vst 0, %[v1]\n"
		     " vst 1, %[v2]\n"
		     " vst 2, %[v3]\n"
		     " vst 3, %[v4]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v),
		       [v4]  "+Q" (v4->v)
		     : : "v0", "v1", "v2", "v3");
}

static void test_vperm(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0011223344556677ull, /* 0 - 7 */
		.q[1] = 0x8899aabbccddeeffull, /* 8 - 15 */
	};
	vector v3 = {
		.q[0] = 0x7766554433221100ull, /* 16 - 23 */
		.q[1] = 0xffeeddccbbaa9988ull, /* 24 - 31 */
	};
	vector v4 = {
		.h[0] = 31,
		.h[1] = 7,
		.h[2] = 32, /* wrapped to 0, 0x11 */
		.h[3] = 27,
		.h[4] = 28,
		.h[5] = 29,
		.h[6] = 30,
		.h[7] = 31,
		.h[8] = 16,
		.h[9] = 17,
		.h[10] = 18,
		.h[11] = 19,
		.h[12] = 8,
		.h[13] = 9,
		.h[14] = 10,
		.h[15] = 11,
	};

	vperm(&v1, &v2, &v3, &v4);
	report("vperm", v1.q[0] == 0x887700ccbbaa9988ull &&
			v1.q[1] == 0x776655448899aabbull);
}

static inline void vpdi(vector *v1, vector *v2, vector *v3, uint8_t m4)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vpdi 1, 2, 3, %[m4]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v)
		     : [m4]  "i" (m4)
		     : "v1", "v2", "v3");
}

static void test_vpdi(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0011223344556677ull,
		.q[1] = 0x8899aabbccddeeffull,
	};
	vector v3 = {
		.q[0] = 0x7766554433221100ull,
		.q[1] = 0xffeeddccbbaa9988ull,
	};

	report_prefix_push("vpdi");

	vpdi(&v1, &v2, &v3, 0x0);
	report("load highest", v1.q[0] == 0x0011223344556677ull &&
			       v1.q[1] == 0x7766554433221100ull);

	vpdi(&v1, &v2, &v3, 0x5);
	report("load lowest", v1.q[0] == 0x8899aabbccddeeffull &&
			      v1.q[1] == 0xffeeddccbbaa9988ull);

	vpdi(&v1, &v2, &v3, 0x1);
	report("load mixed", v1.q[0] == 0x0011223344556677ull &&
			     v1.q[1] == 0xffeeddccbbaa9988ull);

	report_prefix_pop();
}

static inline void vpk(vector *v1, vector *v2, vector *v3, uint8_t m4)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vpk 1, 2, 3, %[m4]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v)
		     : [m4]  "i" (m4)
		     : "v1", "v2", "v3");
}

static void test_vpk(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0000111122223333ull,
		.q[1] = 0x4444555566667777ull,
	};
	vector v3 = {
		.q[0] = 0x88889999aaaabbbbull,
		.q[1] = 0xccccddddeeeeffffull,
	};

	report_prefix_push("vpk");

	vpk(&v1, &v2, &v3, 1);
	report("16 -> 8", v1.q[0] == 0x0011223344556677ull &&
			  v1.q[1] == 0x8899aabbccddeeffull);
	printf("%"PRIx64"\n", v1.q[0]);
	printf("%"PRIx64"\n", v1.q[1]);

	vpk(&v1, &v2, &v3, 2);
	report("32 -> 16", v1.q[0] == 0x1111333355557777ull &&
			   v1.q[1] == 0x9999bbbbddddffffull);

	vpk(&v1, &v2, &v3, 3);
	report("64 -> 32", v1.q[0] == 0x2222333366667777ull &&
			   v1.q[1] == 0xaaaabbbbeeeeffffull);

	/* reserved ES */
	expect_pgm_int();
	vpk(&v1, &v2, &v3, 0);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);
	expect_pgm_int();
	vpk(&v1, &v2, &v3, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline int vpks(vector *v1, vector *v2, vector *v3, uint8_t m4,
		       uint8_t m5)
{
	int cc = 0;

	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vpks 1, 2, 3, %[m4], %[m5]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     " ipm %[cc]\n"
		     " srl %[cc],28\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v),
		       [cc]  "=d" (cc)
		     : [m4]  "i" (m4),
		       [m5]  "i" (m5)
		     : "v1", "v2", "v3", "cc");

	return cc;
}

static void test_vpks(void)
{
	vector v1 = {};
	vector v2 = {};
	vector v3 = {};
	int cc;

	report_prefix_push("vpks");

	cc = vpks(&v1, &v2, &v3, 1, 1);
	report("16 -> 8, no sat", v1.q[0] == 0 && v1.q[1] == 0 && cc == 0);
	cc = vpks(&v1, &v2, &v3, 2, 1);
	report("32 -> 16, no sat", v1.q[0] == 0 && v1.q[1] == 0 && cc == 0);
	cc = vpks(&v1, &v2, &v3, 3, 1);
	report("64 -> 32, no sat", v1.q[0] == 0 && v1.q[1] == 0 && cc == 0);

	v2.q[0] = 0;
	v2.q[1] = (uint16_t)(INT16_MIN);
	v3.q[0] = 0;
	v3.q[1] = (uint16_t)(INT16_MAX);
	cc = vpks(&v1, &v2, &v3, 1, 1);
	report("16 -> 8, some sat", v1.q[0] == (uint8_t)INT8_MIN &&
				    v1.q[1] == (uint8_t)INT8_MAX && cc == 1);

	v2.q[0] = 0;
	v2.q[1] = (uint32_t)(INT32_MIN);
	v3.q[0] = 0;
	v3.q[1] = (uint32_t)(INT32_MAX);
	cc = vpks(&v1, &v2, &v3, 2, 1);
	report("32 -> 16, some sat", v1.q[0] == (uint16_t)INT16_MIN &&
				     v1.q[1] == (uint16_t)INT16_MAX && cc == 1);

	v2.q[0] = 0;
	v2.q[1] = (uint64_t)(INT64_MIN);
	v3.q[0] = 0;
	v3.q[1] = (uint64_t)(INT64_MAX);
	cc = vpks(&v1, &v2, &v3, 3, 1);
	report("64 -> 32, some sat", v1.q[0] == (uint32_t)INT32_MIN &&
				     v1.q[1] == (uint32_t)INT32_MAX && cc == 1);

	v2.q[0] = (uint64_t)(INT64_MIN);
	v2.q[1] = (uint64_t)(INT64_MIN);
	v3.q[0] = (uint64_t)(INT64_MAX);
	v3.q[1] = (uint64_t)(INT64_MAX);
	cc = vpks(&v1, &v2, &v3, 3, 1);
	report("64 -> 32, all sat", cc == 3);

	/* reserved ES */
	expect_pgm_int();
	vpks(&v1, &v2, &v3, 0, 0);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);
	expect_pgm_int();
	vpks(&v1, &v2, &v3, 4, 0);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void vuph(vector *v1, vector *v2, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vuph 1, 2, %[m3]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v)
		     : [m3]  "i" (m3)
		     : "v1", "v2");
}

static inline void vuplh(vector *v1, vector *v2, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vuplh 1, 2, %[m3]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v)
		     : [m3]  "i" (m3)
		     : "v1", "v2");
}

static inline void vupl(vector *v1, vector *v2, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vupl 1, 2, %[m3]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v)
		     : [m3]  "i" (m3)
		     : "v1", "v2");
}

static inline void vupll(vector *v1, vector *v2, uint8_t m3)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vupll 1, 2, %[m3]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v)
		     : [m3]  "i" (m3)
		     : "v1", "v2");
}

static void test_vup(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0000111122223333ull,
		.q[1] = 0x4444555566667777ull,
	};

	report_prefix_push("vup");

	v2.q[0] = 0x00112233ccddeeffull;
	v2.q[1] = 0;
	vuph(&v1, &v2, 0);
	report("high: 8 -> 16", v1.q[0] == 0x0000001100220033ull &&
				v1.q[1] == 0xffccffddffeeffffull);
	vuplh(&v1, &v2, 0);
	report("logical high: 8 -> 16", v1.q[0] == 0x0000001100220033ull &&
					v1.q[1] == 0x00cc00dd00ee00ffull);

	v2.q[0] = 0;
	v2.q[1] = 0x00112233ccddeeffull;
	vupl(&v1, &v2, 0);
	report("low: 8 -> 16", v1.q[0] == 0x0000001100220033ull &&
			       v1.q[1] == 0xffccffddffeeffffull);
	vupll(&v1, &v2, 0);
	report("logical low: 8 -> 16", v1.q[0] == 0x0000001100220033ull &&
				       v1.q[1] == 0x00cc00dd00ee00ffull);

	v2.q[0] = 0x00001111cccceeeeull;
	v2.q[1] = 0;
	vuph(&v1, &v2, 1);
	report("high: 16 -> 32", v1.q[0] == 0x0000000000001111ull &&
				 v1.q[1] == 0xffffccccffffeeeeull);
	vuplh(&v1, &v2, 1);
	report("logical high: 16 -> 32", v1.q[0] == 0x0000000000001111ull &&
					 v1.q[1] == 0x0000cccc0000eeeeull);

	v2.q[0] = 0;
	v2.q[1] = 0x00001111cccceeeeull;
	vupl(&v1, &v2, 1);
	report("low: 16 -> 32", v1.q[0] == 0x0000000000001111ull &&
				v1.q[1] == 0xffffccccffffeeeeull);
	vupll(&v1, &v2, 1);
	report("logical low: 16 -> 32", v1.q[0] == 0x0000000000001111ull &&
					v1.q[1] == 0x0000cccc0000eeeeull);

	v2.q[0] = 0x11111111eeeeeeeeull;
	v2.q[1] = 0;
	vuph(&v1, &v2, 2);
	report("high: 32 -> 64", v1.q[0] == 0x0000000011111111ull &&
				 v1.q[1] == 0xffffffffeeeeeeeeull);
	vuplh(&v1, &v2, 2);
	report("logical high: 32 -> 64", v1.q[0] == 0x0000000011111111ull &&
					 v1.q[1] == 0x00000000eeeeeeeeull);

	v2.q[0] = 0;
	v2.q[1] = 0x11111111eeeeeeeeull;
	vupl(&v1, &v2, 2);
	report("low: 32 -> 64", v1.q[0] == 0x0000000011111111ull &&
				v1.q[1] == 0xffffffffeeeeeeeeull);
	vupll(&v1, &v2, 2);
	report("logical low: 32 -> 64", v1.q[0] == 0x0000000011111111ull &&
					v1.q[1] == 0x00000000eeeeeeeeull);

	/* reserved ES */
	expect_pgm_int();
	vuph(&v1, &v2, 3);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	vuplh(&v1, &v2, 3);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	vupl(&v1, &v2, 3);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	vupll(&v1, &v2, 3);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline void vsrl(vector *v1, vector *v2, vector *v3)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vsrl 1, 2, 3\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v)
		     : : "v1", "v2", "v3");
}

static void test_vsrl(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0011223344556677ull,
		.q[1] = 0x8899aabbccddeeffull,
	};
	vector v3 = { };
	int i;

	report_prefix_push("vsrl");

	for (i = 0; i < 16; i++) {
		v3.h[i] = 0;
	}
	vsrl(&v1, &v2, &v3);
	report(">> 0", v1.q[0] == 0x0011223344556677ull &&
		       v1.q[1] == 0x8899aabbccddeeffull);

	for (i = 0; i < 16; i++) {
		v3.h[i] = 4;
	}
	vsrl(&v1, &v2, &v3);
	report(">> 4", v1.q[0] == 0x0001122334455667ull &&
		       v1.q[1] == 0x78899aabbccddeefull);

	/* only some bits are considered */
	for (i = 0; i < 16; i++) {
		v3.h[i] = 12;
	}
	vsrl(&v1, &v2, &v3);
	report(">> (12 & 0x7)", v1.q[0] == 0x0001122334455667ull &&
				v1.q[1] == 0x78899aabbccddeefull);

	report_prefix_pop();
}

static inline void vsrlb(vector *v1, vector *v2, vector *v3)
{
	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vsrlb 1, 2, 3\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v)
		     : : "v1", "v2", "v3");
}

static void test_vsrlb(void)
{
	vector v1 = {};
	vector v2 = {
		.q[0] = 0x0011223344556677ull,
		.q[1] = 0x8899aabbccddeeffull,
	};
	vector v3 = { };

	report_prefix_push("vsrlb");

	v3.h[7] = 0;
	vsrlb(&v1, &v2, &v3);
	report(">> 0", v1.q[0] == 0x0011223344556677ull &&
		       v1.q[1] == 0x8899aabbccddeeffull);

	v3.h[7] = 16;
	vsrlb(&v1, &v2, &v3);
	report(">> 16", v1.q[0] == 0x0000001122334455ull &&
		        v1.q[1] == 0x66778899aabbccddull);

	v3.h[7] = 64;
	vsrlb(&v1, &v2, &v3);
	report(">> 16", v1.q[0] == 0 && v1.q[1] == 0x0011223344556677ull);

	v3.h[7] = 120;
	vsrlb(&v1, &v2, &v3);
	report(">> 120", v1.q[0] == 0 && v1.q[1] == 0);

	/* some bits are ignored */
	v3.h[7] = 199;
	vsrlb(&v1, &v2, &v3);
	report(">> (199 & 0x78)", v1.q[0] == 0 &&
				  v1.q[1] == 0x0011223344556677ull);

	report_prefix_pop();
}

static inline int vfee(vector *v1, vector *v2, vector *v3, uint8_t m4,
		       uint8_t m5)
{
	int cc = 0;

	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vfee 1, 2, 3, %[m4], %[m5]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     " ipm %[cc]\n"
		     " srl %[cc],28\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v),
		       [cc]  "=d" (cc)
		     : [m4]  "i"  (m4),
		       [m5]  "i"  (m5)
		     : "v1", "v2", "v3", "cc");

	return cc;
}

static void test_vfee(void)
{
	vector v1 = {
		.q[0] = -1ull,
		.q[1] = -1ull,
	};
	vector v2;
	vector v3;
	int cc;

	report_prefix_push("vfee");

	v2.q[0] = 0x2222222222222222ull;
	v2.q[1] = 0x2222222222222222ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1111111111111111ull;
	cc = vfee(&v1, &v2, &v3, 0, 1);
	report("unequal", cc == 3 && v1.h[7] == 16);
	cc = vfee(&v1, &v2, &v3, 0, 3);
	report("unequal, not zero", cc == 3 && v1.h[7] == 16);

	v2.q[0] = 0x2222222222220022ull;
	v2.q[1] = 0x2200222222222200ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x0011111111111111ull;
	cc = vfee(&v1, &v2, &v3, 0, 3);
	report("unequal, zero in first", cc == 0 && v1.h[7] == 6);

	v2.q[0] = 0x2222222222222222ull;
	v2.q[1] = 0x2222222222222222ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x0011111111110011ull;
	cc = vfee(&v1, &v2, &v3, 0, 3);
	report("unequal, zero in second", cc == 3 && v1.h[7] == 16);

	v2.q[0] = 0x2222222222222222ull;
	v2.q[1] = 0x2222222200222222ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1111111111112211ull;
	cc = vfee(&v1, &v2, &v3, 0, 1);
	report("equal", cc == 1 && v1.h[7] == 14);
	cc = vfee(&v1, &v2, &v3, 0, 3);
	report("zero before equal", cc == 0 && v1.h[7] == 12);

	v2.q[0] = 0x2222221122222222ull;
	v2.q[1] = 0x2222222222222200ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1111111111111111ull;
	cc = vfee(&v1, &v2, &v3, 0, 3);
	report("zero after equal", cc == 2 && v1.h[7] == 3);

	report("vector cleared", v1.q[1] == 0);

	/* reserved ES */
	expect_pgm_int();
	vfee(&v1, &v2, &v3, 3, 0);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);
	/* reserved bits */
	expect_pgm_int();
	vfee(&v1, &v2, &v3, 0, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}

static inline int vfene(vector *v1, vector *v2, vector *v3, uint8_t m4,
		        uint8_t m5)
{
	int cc = 0;

	asm volatile(" .machine z13\n"
		     " vl 1, %[v1]\n"
		     " vl 2, %[v2]\n"
		     " vl 3, %[v3]\n"
		     " vfene 1, 2, 3, %[m4], %[m5]\n"
		     " vst 1, %[v1]\n"
		     " vst 2, %[v2]\n"
		     " vst 3, %[v3]\n"
		     " ipm %[cc]\n"
		     " srl %[cc],28\n"
		     : [v1]  "+Q" (v1->v),
		       [v2]  "+Q" (v2->v),
		       [v3]  "+Q" (v3->v),
		       [cc]  "=d" (cc)
		     : [m4]  "i"  (m4),
		       [m5]  "i"  (m5)
		     : "v1", "v2", "v3", "cc");

	return cc;
}

static void test_vfene(void)
{
	vector v1 = {
		.q[0] = -1ull,
		.q[1] = -1ull,
	};
	vector v2;
	vector v3;
	int cc;

	report_prefix_push("vfene");

	v2.q[0] = 0x1111111111111111ull;
	v2.q[1] = 0x1111111111111111ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1111111111111111ull;
	cc = vfene(&v1, &v2, &v3, 0, 1);
	report("equal without zero", cc == 3 && v1.h[7] == 16);
	cc = vfene(&v1, &v2, &v3, 0, 3);
	report("equal without zero", cc == 3 && v1.h[7] == 16);

	v2.q[0] = 0x1111111111111111ull;
	v2.q[1] = 0x1100111111110011ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1100111111110011ull;
	cc = vfene(&v1, &v2, &v3, 0, 1);
	report("equal with zero", cc == 3 && v1.h[7] == 16);
	cc = vfene(&v1, &v2, &v3, 0, 3);
	report("equal with zero", cc == 0 && v1.h[7] == 9);

	v2.q[0] = 0x1111111111111111ull;
	v2.q[1] = 0x1100111311110011ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1100111111110011ull;
	cc = vfene(&v1, &v2, &v3, 0, 3);
	report("unequal after zero", cc == 0 && v1.h[7] == 9);

	v2.q[0] = 0x1111101111111111ull;
	v2.q[1] = 0x1100111111110011ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1100111111110011ull;
	cc = vfene(&v1, &v2, &v3, 0, 1);
	report("smaller before zero", cc == 1 && v1.h[7] == 2);
	cc = vfene(&v1, &v2, &v3, 0, 3);
	report("smaller before zero", cc == 1 && v1.h[7] == 2);

	v2.q[0] = 0x1111111111111141ull;
	v2.q[1] = 0x1100111111110011ull;
	v3.q[0] = 0x1111111111111111ull;
	v3.q[1] = 0x1100111111110011ull;
	cc = vfene(&v1, &v2, &v3, 0, 1);
	report("bigger before zero", cc == 2 && v1.h[7] == 7);
	cc = vfene(&v1, &v2, &v3, 0, 3);
	report("bigger before zero", cc == 2 && v1.h[7] == 7);

	report("vector cleared", v1.q[1] == 0);

	/* reserved ES */
	expect_pgm_int();
	vfene(&v1, &v2, &v3, 3, 0);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);
	/* reserved bits */
	expect_pgm_int();
	vfene(&v1, &v2, &v3, 0, 4);
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	report_prefix_pop();
}


/* z14 vector extension test */
static void test_ext1_nand(void)
{
	bool has_vext = test_facility(134);
	static struct prm {
		__uint128_t a,b,c;
	} prm __attribute__((aligned(16)));

	if (!has_vext) {
		report_skip("Vector extensions 1 is not available");
		return;
	}

	memset(&prm, 0xff, sizeof(prm));

	asm volatile(" .machine z13\n"
		     " vl 0, %[v1]\n"
		     " vl 1, %[v2]\n"
		     " .byte 0xe7, 0x20, 0x10, 0x00, 0x00, 0x6e\n" /* vnn */
		     " vst 2, %[v3]\n"
		     : [v3]  "=Q" (prm.c)
		     : [v1]  "Q" (prm.a), [v2]  "Q" (prm.b)
		     : "v0", "v1", "v2", "memory");
	report("nand ff", !prm.c);
}

/* z14 bcd extension test */
static void test_bcd_add(void)
{
	bool has_bcd = test_facility(135);
	static struct prm {
		__uint128_t a,b,c;
	} prm __attribute__((aligned(16)));

	if (!has_bcd) {
		report_skip("Vector BCD extensions is not available");
		return;
	}

	prm.c = 0;
	prm.a = prm.b = 0b001000011100;

	asm volatile(" .machine z13\n"
		     " vl 0, %[v1]\n"
		     " vl 1, %[v2]\n"
		     " .byte 0xe6, 0x20, 0x10, 0x01, 0x00, 0x71\n" /* vap */
		     " vst 2, %[v3]\n"
		     : [v3]  "=Q" (prm.c)
		     : [v1]  "Q" (prm.a), [v2]  "Q" (prm.b)
		     : "v0", "v1", "v2", "memory");
	report("bcd add 21", prm.c == 0x42c);
}

static void init(void)
{
	/* Enable vector instructions */
	ctl_set_bit(0, 17);

	/* Preset vector registers to 0xff */
	memset(pagebuf, 0xff, PAGE_SIZE);
	vlm_all((u64*)pagebuf);
}

int main(void)
{
	bool has_vregs = test_facility(129);

	report_prefix_push("vector");
	if (!has_vregs) {
		report_skip("Basic vector facility is not available");
		goto done;
	}

	init();
	test_vgef();
	test_vgeg();
	test_vgbm();
	test_vgm();
	test_vlrep();
	test_vlgv();

	test_va();
	test_vlr();
	test_vperm();
	test_vpdi();
	test_vpk();
	test_vpks();
	test_vup();
	test_vsrl();
	test_vsrlb();
	test_vgfma();
	test_vfee();
	test_vfene();
	test_ext1_nand();
	test_bcd_add();

done:
	report_prefix_pop();
	return report_summary();
}
