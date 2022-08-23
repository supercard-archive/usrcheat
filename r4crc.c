#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

typedef void (ifn)(uint32_t*);
typedef void (ufn)(uint32_t*, const uint8_t *, size_t);
typedef uint32_t (ffn)(uint32_t*);

/* crc32 algo as used in R4 DS flash cartridge usrcheat.dat.
   it's actually known as CRC-32/JAMCRC. only first 512bytes
   of the ROM are checked. */

static void crc32_r4_init(uint32_t *ctx) {
	*ctx = -1u;
}

static void crc32_r4_update(uint32_t *ctx, const uint8_t *p, size_t len) {
#define CRCPOLY 0xedb88320
	unsigned i;
	uint32_t crc = *ctx;
	while(len--)
	{
		crc ^= *p++;
		for(i=0; i<8; ++i)
			crc=(crc>>1)^((crc&1)?CRCPOLY:0);
	}
	*ctx = crc;
}

static uint32_t crc32_r4_final(uint32_t *ctx) {
	return *ctx;
}

static uint32_t selftest(ifn* init, ufn* feed, ffn *final)
{
	uint32_t ctx;
	init(&ctx);
	feed(&ctx, "123456789", 9);
	return final(&ctx);
}

static int usage(void) {
	printf(
		"r4crc ROMFILE\n"
		"prints crc32 of ROMFILE using the algorithm used for usrcheat.dat and cheats.xml\n"
		);
	return 1;
}

int main(int argc, char **argv) {

	if(argc != 2) return usage();

	uint32_t r4ctx, check;

	/* https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat-bits.32  */
	check = selftest(crc32_r4_init, crc32_r4_update, crc32_r4_final);
	assert(check == 0x340bc6d9);

	crc32_r4_init(&r4ctx);

	FILE *f = fopen(argv[1], "rb");
	if(!f) {
		perror("fopen");
		return 1;
	}

	unsigned char buf[512];
	size_t ret = fread(buf, 1, 512, f);
	fclose(f);
	if(ret != 512) {
		perror("fread");
		return 1;
	}

	crc32_r4_update(&r4ctx, buf, 512);
	printf("CRC32: %08x\n", crc32_r4_final(&r4ctx));

	return 0;
}
