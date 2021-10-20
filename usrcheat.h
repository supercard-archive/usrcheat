#include <stdint.h>

enum encoding {
	ENC_NONE = -1,
	ENC_GBK  = 0,
	ENC_BIG5 = 1,
	ENC_SJIS = 2,
	ENC_UTF8 = 3,
};

/* action replay off/val tuple */
struct archeat {
	uint32_t off;
	uint32_t val;
};

#define GAME_FLAG_ENABLED 0xf0
#define CHEATITEM_FLAG_DIRECTORY 0x1000
#define CHEATITEM_FLAG_ONEHOT 0x0100 /* if this flag is set, only one cheat can be enabled - should be used only for dirs*/
#define CHEAT_FLAG_ENABLED 0x0100 /* a cart writes this flag if user enabled the cheat, so it survives reboot */

struct cheatitem {
	uint16_t flags;
	uint32_t n;
	char *desc;
	char *note;
	struct archeat *codes;
};

struct game {
	char *name;
	char id[4];
	uint32_t crc;
	uint32_t mastercode[8]; /* no idea what this is, but it defaults to all 0 except 1 for 2nd item */
	uint16_t flags;
	uint16_t n_items;
	struct cheatitem *items;
};

struct cheatdb {
	char name[0x3b];
	uint32_t n_games;
	struct game *games;
	int enabled;
	enum encoding encoding;
};

int cheatdb_read(struct cheatdb *res, char* fn, char **error);
