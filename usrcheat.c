#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <iconv.h>
#include "usrcheat.h"
#include "endianness.h"

#define VERSION "1.0.0"

#define DUP_CHECK
#undef MASTERCODE_CHECK
#ifdef DEBUG
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DPRINTF(...) do {} while(0)
#endif

struct gameinfo {
	char id[4];
	uint32_t crc;
	uint64_t off;
};

static const uint32_t def_mastercode[8] = { 0, 1 };

static int read_string(FILE *f, char *buf, size_t buflen, int align) {
	unsigned n = 0;
	int c;
	while((c = fgetc(f)) >= 0) {
		if(c == EOF) return 0;
		buf[n++] = c;
		if(!c) break;
		if(n >= buflen) return 0;
	}
	if(align) while(ftello(f) & 3) {
		c = fgetc(f);
		if(c == EOF) return 0;
		assert(c == 0);
	}
	return 1;
}

static char* id2str(char *id) {
	static char buf[5]; /* FIXME: not thread-safe, in case this will be remodeled into a library */
	buf[4] = 0;
	memcpy(buf, id, 4);
	return buf;
}

static int startswith(const char *p, const char *what) {
	unsigned i, l = strlen(what);
	for(i=0; i<l; ++i) if(p[i] != what[i]) return 0;
	return 1;
}

/* FIXME: static buffer not threadsafe */
static char encbuf[8192];

static char* sanitize_for_dat(char*s) {
	static const struct {
		char in[3];
		char out[1];
	} replacement_tab[] = {
		/* change back «» to <> respectively */
		{ "\302\253", "<" },                // «
		{ "\302\273", ">" },                // »
	};
	char *p = s ? s : "";
	char *q = encbuf, *e = encbuf+sizeof(encbuf)-1;
	unsigned i;
	while(*p) {
		for(i=0; i<2; ++i)
			if(startswith(p, replacement_tab[i].in)) {
				*q = replacement_tab[i].out[0];
				p++;
				break;
			}
		if(i == 2) *q = *p;
		q++; p++;
		if(q >= e) break;
	}
	*q = 0;
	return encbuf;
}

/* remove non-ascii chars and '<' '>' which would confuse xml parsers */
static char* sanitize_for_xml(char *s, enum encoding enc) {
	static const struct {
		char in[5];
		char out[3];
	} replacement_tab[] = {
		/* we need to replace < > in order to not mess up xml */
		{ "<", "\302\253" },                // «
		{ ">", "\302\273" },                // »
		/* replace some garbage characters contained in GBAtemp_26-03-11 usrcheat.dat release
		   the file declares encoding as GBK but that just produces
		   chinese chars inside european words.
		   we replace them with ascii chars rather than the correct
		   utf-8 runes, in order to prevent further issues when
		   converting back and forth. */
		{ "\245\243\245\362", "e" /*"\303\251"*/ }, // é
		{ "\251b", "ss" /*"\303\237"*/ },           // ß
		{ "\245\243\241\242", "a" /*"\303\240"*/ }, // à
		{ "\245\243\245\250", "u" /*"\303\271"*/ }, // ù
		{ "\241\243\245\303", "'" /*"\302\264"*/ }, // ´
		{ "\241\243?", "#" /*"\302\260"*/ },        // °
		{ "\245\243\245\243", "e" /*"\303\250"*/ }, // è
		{ "\245\243\245\263", "o" /*"\303\264"*/ }, // ô
		{ "\361w", "e" /*"\303\251"*/ },            // é
	};
	unsigned i;
	signed char *p = (void*) s;
	signed char *q = (void*) encbuf, *e = encbuf+sizeof(encbuf)-1;
	int ret = 0, nc = 0;
	/* step 1: replace from known bad character table */
	while(*p) {
		if(*p < ' ' || *p == '<' || *p == '>') {
			const int tab_count = sizeof(replacement_tab)/sizeof(replacement_tab[0]);
			for(i = 0; i < tab_count; ++i) {
				if(startswith(p, replacement_tab[i].in)) {
					strcpy(q, replacement_tab[i].out);
					p+=strlen(replacement_tab[i].in)-1;
					q+=strlen(replacement_tab[i].out)-1;
					break;
				}
				/* only process < and > if encoding is already utf-8 */
				if(i >= 1 && enc == ENC_UTF8) break;
			}
			if(i == tab_count) { *q = *p ;	++nc; }
		} else
			*q = *p;
		++p; ++q;
		if(q >= e) break;
	}
	*q = 0;
	if(nc && enc != ENC_UTF8 && enc != ENC_NONE) {
		/* step 2: if bad chars were found outside of step1, try to
		   convert to the declared encoding. */
		static const char enc_to_name[][6] = {
			[ENC_GBK]  = "GBK",
			[ENC_BIG5] = "BIG5",
			[ENC_SJIS] = "SJIS",
			[ENC_UTF8] = "UTF-8",
		};
		iconv_t cd = iconv_open("UTF-8", getenv("USRCHEAT_ENCODING") ? getenv("USRCHEAT_ENCODING") : enc_to_name[enc]);
		if(cd == (iconv_t)-1) goto fallback;
		size_t inlen, outlen;
		inlen = strlen(encbuf);
		outlen = inlen*4;
		char *convbuf = calloc(inlen+1, 4);
		char *in = encbuf, *out = convbuf;
		size_t ret = iconv(cd, &in, &inlen, &out, &outlen);
		iconv_close(cd);
		if(ret == (size_t)-1) {
		fallback:
			/* step 3: if step 2 failed, try windows legacy enc. */
			cd = iconv_open("UTF-8", "ISO-8859-1");
			if(cd == (iconv_t)-1) {
			e_2:
				free(convbuf);
				goto brutus;
			}
			inlen = strlen(encbuf);
			outlen = inlen*4;
			in = encbuf; out = convbuf;
			ret = iconv(cd, &in, &inlen, &out, &outlen);
			iconv_close(cd);
			if(ret == (size_t)-1) goto e_2;
		}
		snprintf(encbuf, sizeof(encbuf)-1, "%s", convbuf);
		free(convbuf);
	} else if(nc && enc == ENC_NONE) {
		/* if everything else failed, overwrite the garbage with ~ */
	brutus:
		q = (void*) encbuf;
		while(*q) {
			if(*q < ' ') *q = '~';
			++q;
		}
	}
	return encbuf;
}

static int read_game(FILE *f, char **error, struct gameinfo *gi, struct game *game) {
	off_t cp = ftello(f);
	if(fseeko(f, gi->off, SEEK_SET) == -1) {
		*error = "failure seeking to game data";
		return 0;
	}
	memcpy(game->id, gi->id, 4);
	memcpy(&game->crc, &gi->crc, 4);
	char namebuf[1024];
	if(!read_string(f, namebuf, sizeof namebuf, 1)) {
		*error = "eof while reading string";
		return 0;
	}
	uint16_t n_items;
	fread(&n_items, 1, 2, f);
	n_items = end_le16toh(n_items);
	fread(&game->flags, 1, 2, f);
	game->flags = end_le16toh(game->flags);
	uint32_t mastercode[8], i;
	if(8 != fread(mastercode, 4, 8, f)) {
		*error = "short read (mastercode)";
		return 0;
	}
	for(i = 0; i < 8; ++i) game->mastercode[i] = end_le32toh(mastercode[i]);
#ifdef MASTERCODE_CHECK
	static const uint32_t def_mastercode[8] = { 0, 1 };
	if(memcmp(mastercode, def_mastercode, 8*4)) {
		fprintf(stderr, "warning: game %s uses non-default mastercode\n", id2str(game->id));
	}
#endif
	game->n_items = n_items;
	/* start allocating in good faith */
	game->name = strdup(namebuf);
	game->items = calloc(n_items, sizeof (struct cheatitem));
	unsigned dircount = 0;
	for(i = 0; i < n_items; ++i) {
		union {
			uint32_t i;
			uint16_t w[2];
		} b4;
		if(4 != fread(&b4, 1, 4, f)) {
			*error = "short read (item flags)";
		err_free:
			/* TODO: free items in game->items[0-i] */
			free(game->name);
			free(game->items);
			return 0;
		}

		char name[1024];
		char note[1024];
		if(!read_string(f, name, sizeof name, 0) ||
		   !read_string(f, note, sizeof note, 1)) {
			*error = "short read or unsufficient buf size (item desc)";
			goto err_free;
		}
		struct cheatitem *item = &game->items[i];
		item->n = end_htole16(b4.w[0]);
		item->flags = end_htole16(b4.w[1]);
		item->desc = strdup(name);
		item->note = strdup(note);

		if(end_htole16(b4.w[1]) & CHEATITEM_FLAG_DIRECTORY) {
			if(dircount) {
				*error = "nested dirs";
				goto err_free;
			}
			dircount = item->n;
		} else {
			if(dircount) --dircount;
			if(4 != fread(&b4, 1, 4, f)) {
				*error = "short read (code len)";
				goto err_free;
			}
			unsigned n_codes = end_htole32(b4.i) & 0xffffff;
			if (n_codes & 1) {
				fprintf(stderr, "warning: odd number of cheat words in cheat %s, game id %s\n", name, id2str(gi->id));
			}
			/* for a cheat code item, flags are used from previous
			   read, but item count is provided as a separate 32bit counter */
			item->n = n_codes;
			if (n_codes) {
				n_codes /= 2;
				item->codes = calloc(n_codes, sizeof(struct archeat));
				if(n_codes != fread(item->codes, sizeof(struct archeat), n_codes, f)) {
					*error = "error while reading cheat data";
					free(item->codes);
					goto err_free;
				}
				if(item->n & 1) {
					uint32_t waste;
					fread(&waste, 1, 4, f);
					waste = end_le32toh(waste);
					fprintf(stderr, "cut-off cheat word: %08x at pos %lx\n", waste, ftello(f)-4L);
				}
				item->n = n_codes;

				unsigned j;
				for(j = 0; j < n_codes; ++j) {
					struct archeat *p = &item->codes[j];
					p->off = end_le32toh(p->off);
					p->val = end_le32toh(p->val);
				}
			}
			DPRINTF("loc after item: %lx\n", (long) ftello(f));
		}
	}
	DPRINTF("loc after game: %lx\n", (long) ftello(f));
	if(dircount) {
		*error = "imprecise number of dir entries";
		goto err_free;
	}
	if(fseeko(f, cp, SEEK_SET) == -1) goto err_free;
	return 1;
}

/* these codes were reverse engineered thanks to xperia64's Jusrcheat.
   they spell the name Yasu of the developer of R4CCE in different forms */
static const unsigned char encoding_map[][4] = {
	[ENC_GBK] = "\xD5\x53\x41\x59",
	[ENC_BIG5] = "\xF5\x53\x41\x59",
	[ENC_SJIS] = "\x75\x53\x41\x59",
	[ENC_UTF8] = "\x55\x73\x41\x59",
};

static void set_encoding(enum encoding enc, unsigned char* out) {
	if(enc == ENC_NONE) memcpy(out, "\0\0\0\0", 4);
	else memcpy(out, encoding_map[enc], 4);
}

static enum encoding get_encoding(unsigned char* in) {
	unsigned i;
	for(i=0; i < 4; ++i)
		if(!memcmp(encoding_map[i], in, 4)) break;
	if(i == 4) return ENC_NONE;
	return i;
}

static const struct cheatdb db_empty = {0};

int cheatdb_read(struct cheatdb *res, char* fn, char **error) {
	memcpy(res, &db_empty, sizeof db_empty);

	FILE *f = fopen(fn, "r");
	if(!f) {
		*error = "can't open file";
		return 0;
	}
	unsigned char header[0x100];
	if(sizeof(header) != fread(header, 1, sizeof header, f)) {
		*error = "short read";
	err_f:
		fclose(f);
		return 0;
	}
	if(memcmp(header, "R4 CheatCode", 12)) {
		*error = "invalid header magic";
		goto err_f;
	}
	uint32_t h_len;
	memcpy(&h_len, header+12, 4);
	h_len = end_le32toh(h_len);
	if(h_len != 0x100) {
		*error = "unexpected header length";
		goto err_f;
	}
	res->enabled = header[0x50];
	res->encoding = get_encoding(header+0x50-4);
	memcpy(res->name, header+0x10, 0x3b);
	res->name[0x3a] = 0;
	unsigned n_games = 0;
	fseeko(f, 0x100, SEEK_SET);
	for(;;) {
		struct gameinfo gi;
		if(sizeof gi != fread(&gi, 1, sizeof gi, f)) {
			*error = "short read";
			goto err_f;
		}
		if(!memcmp(gi.id, "\0\0\0\0", 4)) break;
		++n_games;
	}
	res->games = calloc(n_games, sizeof(struct game));
	res->n_games = 0;
	unsigned i;
	fseeko(f, 0x100, SEEK_SET);
	for(i=0; i<n_games; ++i) {
		struct gameinfo gi;
		fread(&gi, 1, sizeof gi, f);
#ifdef DUP_CHECK
		unsigned j;
		for(j = 0; j < res->n_games; ++j) {
			if(!memcmp(res->games[j].id, gi.id, 4) && res->games[j].crc == gi.crc) {
				fprintf(stderr, "warning: duplicate game %s\n", id2str(gi.id));
			}
		}
#endif

		if(!read_game(f, error, &gi, &res->games[res->n_games++])) {
			return 0;
		}
	}
	fclose(f);
	return 1;
}

static int write_string(FILE *f, char* s, char** error, int align) {
	if(!s) s = "";
	size_t l = strlen(s)+1;
	if(l != fwrite(s, 1, l, f)) {
	e_w:
		*error = "write error writing string";
		return 0;
	}
	if(align) while(ftello(f) & 3) if(1 != fwrite("", 1, 1, f)) goto e_w;
	return 1;
}

int cheatdb_write(struct cheatdb *db, char* fn, char** error) {
	unsigned char header[0x100] = "R4 CheatCode";
	uint32_t u = end_htole32(0x100);
	unsigned i, j;
	memcpy(header+12, &u, 4);
	memcpy(header+16, db->name, 0x3b);
	FILE *f = fopen(fn, "w");
	if(!f) {
		*error = "can't open file";
		return 0;
	}
	header[0x50] = 1; /* enabled */
	set_encoding(db->encoding, header+0x50-4);

	if(sizeof header != fwrite(header, 1, sizeof header, f)) {
	e_w:
		*error = "write error";
		return 0;
	}
	for (i=0; i<=db->n_games; ++i) {
		if(16 != fwrite("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 1, 16, f))
			goto e_w;
	}
	uint64_t *offsets = calloc(db->n_games+1, sizeof(*offsets));
	for (i=0; i<=db->n_games; ++i) {
		offsets[i] = ftello(f);
		if(i == db->n_games) break;
		struct game *g = db->games+i;
		if(!write_string(f, g->name, error, 1)) return 0;
		uint16_t w = end_htole16(g->n_items);
		if(2 != fwrite(&w, 1, 2, f)) {
		e_fw:
			free(offsets);
			goto e_w;
		}
		w = end_htole16(g->flags);
		if(2 != fwrite(&w, 1, 2, f)) goto e_fw;
		uint32_t mastercode[8], j;
		for(j = 0; j < 8; ++j)
			mastercode[j] = end_htole32(g->mastercode[j]);
		if(8 != fwrite(mastercode, 4, 8, f)) goto e_fw;
		for(j = 0; j < g->n_items; ++j) {
			struct cheatitem *it = g->items+j;
			union {
				uint32_t i;
				uint16_t w[2];
			} b4;
			b4.w[0] = end_htole16(it->n & 0xffff);
			b4.w[1] = end_htole16(it->flags);

			/* this is necessary because we need the sanitized strings twice */
			if(!it->desc) it->desc = strdup("");
			if(!it->note) it->note = strdup("");
			strcpy(it->desc, sanitize_for_dat(it->desc));
			strcpy(it->note, sanitize_for_dat(it->note));

			if(!(it->flags & CHEATITEM_FLAG_DIRECTORY)) {
			/* for the number of cheat words, cheats don't use
			   the count/flag field in front of the cheat name,
			   but a separate counter after the note field.
			   instead, this field is repurposed as a counter
			   for how many blocks of 4 bytes to skip until the
			   next cheat title. */
				b4.i = (strlen(it->desc)+1 + strlen(it->note)+1+3) & ~3;
				b4.i /= 4;
				b4.i += (2*it->n) + 1;
				b4.i = end_htole32(b4.i);
			}
			if(4 != fwrite(&b4, 1, 4, f)) goto e_fw;
			if(!write_string(f, it->desc, error, 0) ||
			   !write_string(f, it->note, error, 1)) goto e_fw;
			if(!(it->flags & CHEATITEM_FLAG_DIRECTORY)) {
				b4.i = end_htole32((2*it->n)&0xffffff);
				if(4 != fwrite(&b4, 1, 4, f)) goto e_fw;
				if(it->n != fwrite(it->codes, sizeof(struct archeat), it->n, f))
					goto e_fw;
			}
		}
	}
	fseeko(f, 0x100, SEEK_SET);
	for (i=0; i<=db->n_games; ++i) {
		struct gameinfo gi;
		/* it would be nice to write into the last empty slot
		   the end offset of the previous record, but alas, that might
		   confuse consumers, and so they have to use special case code
		   to extract filesize for the last offset, if they want to
		   read a single game record into memory */
		if(i == db->n_games)
			/* memset(&gi, 0, sizeof(gi)); */ break;
		else {
			struct game *g = db->games+i;
			memcpy(&gi.id, g->id, 4);
			gi.crc = end_htole32(g->crc);
		}
		gi.off = end_htole64(offsets[i]);
		if(sizeof(gi) != fwrite(&gi, 1, sizeof(gi), f))
			goto e_fw;
	}
	free(offsets);
	fclose(f);
	return 1;
}

#define INDENT indent ? "\t" : ""

static void print_cheat(FILE *f, struct cheatitem *it, int indent) {
	unsigned n;
	fprintf(f, "\t\t\t%s<codes>%s", INDENT, it->flags & CHEAT_FLAG_ENABLED ? "on " : "");
	for(n = 0; n < it->n; ++n) {
		struct archeat* code = it->codes+n;
		fprintf(f, "%08X %08X%s", code->off, code->val, n == it->n-1 ? "</codes>\n" : " ");
	}
}

static void print_game(FILE *f, struct game *g, int encoding) {
	unsigned i;
	fprintf(f, "\t<game>\n\t\t<name>%s</name>\n", sanitize_for_xml(g->name, encoding));
	fprintf(f, "\t\t<gameid>%s %08X</gameid>\n", id2str(g->id), g->crc);
	if(memcmp(g->mastercode, def_mastercode, 8*4)) {
		fprintf(f, "\t\t<cheat>\n\t\t\t<name>(M)MasterCode</name>\n"
		       "\t\t\t<codes>master ");
		for(i=0; i<8; ++i)
			fprintf(f, "00000000 %08X%s", g->mastercode[i], i == 7 ? " 00000000 00000001</codes>\n\t\t</cheat>\n" : " ");
	}

	int indent = 0;
	for(i=0; i<g->n_items; ++i) {
		struct cheatitem *it = g->items+i;
		if(it->flags & CHEATITEM_FLAG_DIRECTORY) {
			fprintf(f, "\t\t<folder>\n\t\t\t<name>%s</name>\n", sanitize_for_xml(it->desc, encoding));
			if(it->note && it->note[0])
				fprintf(f, "\t\t\t<note>%s</note>\n", sanitize_for_xml(it->note, encoding));
			if(it->flags & CHEATITEM_FLAG_ONEHOT)
				fprintf(f, "\t\t\t<allowedon>1</allowedon>\n");
			indent = it->n;
		} else {
			fprintf(f, "\t\t%s<cheat>\n\t\t\t%s<name>%s</name>\n",
				INDENT, INDENT, sanitize_for_xml(it->desc, encoding));
			if(it->note && it->note[0])
				fprintf(f, "\t\t\t%s<note>%s</note>\n", INDENT, sanitize_for_xml(it->note, encoding));
			if(it->n) print_cheat(f, it, indent);
			fprintf(f, "\t\t%s</cheat>\n", INDENT);
			if(indent) {
				if(--indent == 0) fprintf(f, "\t\t</folder>\n");
			}
		}
	}
	fprintf(f, "\t</game>\n");
}

int cheatdb_write_xml(struct cheatdb *db, char *fn, char**error) {
	FILE *f = fopen(fn, "w");
	if(!f) {
		*error = "error opening outfile";
		return 0;
	}
	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<codelist>\n");
	fprintf(f, "\t<name>%s</name>\n", db->name);
	unsigned g;
	for(g = 0; g < db->n_games; ++g) {
		print_game(f, &db->games[g], db->encoding);
	}
	fprintf(f, "</codelist>\n");
	fclose(f);
	return 1;
}

enum TOKEN {
	TOK_NAME = 0,
	TOK_NOTE = 1,
	TOK_GAME = 2,
	TOK_DATE = 3,
	TOK_CHEAT = 4,
	TOK_CODES = 5,
	TOK_FOLDER = 6,
	TOK_GAMEID = 7,
	TOK_CODELIST = 8,
	TOK_ALLOWEDON = 9,
};

static int get_xml_token(char *p) {
	static const char* tokens[] = {
		[TOK_NAME] = "\4name",
		[TOK_NOTE] = "\4note",
		[TOK_GAME] = "\4game",
		[TOK_DATE] = "\4date",
		[TOK_CHEAT] = "\5cheat",
		[TOK_CODES] = "\5codes",
		[TOK_GAMEID] = "\6gameid",
		[TOK_FOLDER] = "\6folder",
		[TOK_CODELIST] = "\010codelist",
		[TOK_ALLOWEDON] = "\011allowedon",
	};
	unsigned i, l = strlen(p);
	for(i=0; i<10; ++i)
		if(tokens[i][0] == l && !memcmp(&tokens[i][1], p, l))
			return i;
	return -1;
}

struct tokstack {
	enum TOKEN stack[32];
	int sp;
};

static int tokstack_pop(struct tokstack *t) {
	if (t->sp == 0) return -1;
	return t->stack[--t->sp];
}

static void tokstack_push(struct tokstack *t, int v) {
	if(t->sp == 32) return;
	t->stack[t->sp++] = v;
}

static int ishexchar(char *p) {
	switch(*p) {
	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case 'a': case 'b':
	case 'c': case 'd': case 'e': case 'f':	case 'A': case 'B':
	case 'C': case 'D': case 'E': case 'F': return 1;
	}
	return 0;
}

int cheatdb_read_xml(struct cheatdb *res, char* fn, char **error, long *lineno) {
#define FETCH() \
	do {	q = p; while(*q && *q != '<') ++q; \
		if(*q != '<') goto e_lt; \
		*(q++) = 0; } while(0)
#define CURRGAME (&res->games[res->n_games-1])
#define CURRITEM (&CURRGAME->items[CURRGAME->n_items-1])

	memcpy(res, &db_empty, sizeof db_empty);
	res->encoding = ENC_UTF8;

	FILE *f = fopen(fn, "r");
	if(!f) {
		*error = "can't open file";
		return 0;
	}

	char linebuf[1024*16]; /* yes, i've seen lines up to 10KB in length..*/
	unsigned long folditems, lastfolder = 0x7fffffff, i, j;
	*lineno = 0;
	struct tokstack ts = {0};
	while(fgets(linebuf, sizeof linebuf, f)) {
		(*lineno)++;
		char *p = linebuf, *q;
		int close_tag;
		while(isspace(*p)) ++p;
		if(*p != '<') {
		e_lt:
			*error = "expected '<'";
			return 0;
		}
		if((close_tag = (*(++p) == '/'))) ++p;
		if(*p == '?') continue;
		q = p;
		while(*q && *q != '>') ++q;
		if(*q != '>') {
		e_gt:
			*error = "expected '>'";
			return 0;
		}
		*(q++) = 0;
		int tok = get_xml_token(p);
		p = q;
		if(tok == -1) {
			*error = "unknown token";
			return 0;
		}
		if(close_tag) {
			if(tokstack_pop(&ts) != tok) {
			e_mmc:
				*error = "mismatching closing tag";
				return 0;
			}
			switch(tok) {
			case TOK_NAME: case TOK_NOTE: case TOK_CODES:
			case TOK_GAMEID: case TOK_ALLOWEDON: case TOK_DATE:
				*error = "closing tag required on same line as opening tag";
				return 0;
			case TOK_FOLDER:
				CURRGAME->items[lastfolder].n = folditems;
				break;
			}
		} else {
			tokstack_push(&ts, tok);
			switch(tok) {
			case TOK_CODELIST:
				assert(ts.sp == 1);
				break;
			case TOK_GAME:
				++res->n_games;
				res->games = realloc(res->games, res->n_games * sizeof (struct game));
				memset(CURRGAME, 0, sizeof(struct game));
				memcpy(CURRGAME->mastercode, def_mastercode, 8*4);
				break;
			case TOK_CHEAT:
			case TOK_FOLDER:
				++CURRGAME->n_items;
				CURRGAME->items = realloc(CURRGAME->items, CURRGAME->n_items*sizeof(struct cheatitem));
				memset(CURRITEM, 0, sizeof(struct cheatitem));
				if(tok == TOK_FOLDER) {
					CURRITEM->flags = CHEATITEM_FLAG_DIRECTORY;
					folditems = 0;
					lastfolder = CURRGAME->n_items-1;
				} else {
					assert(ts.sp >= 2);
					if(ts.stack[ts.sp-2] == TOK_FOLDER)
						folditems++;
				}
				break;
			case TOK_DATE:
				FETCH();
				break;
			case TOK_ALLOWEDON:
				assert(ts.sp >= 2 && ts.stack[ts.sp-2] == TOK_FOLDER);
				FETCH();
				if(atoi(p) == 1) CURRITEM->flags |= CHEATITEM_FLAG_ONEHOT;
				break;
			case TOK_NAME:
				FETCH();
				if(ts.sp == 2) strncpy(res->name, p, sizeof(res->name));
				else if(ts.sp == 3) {
					assert(ts.stack[1] == TOK_GAME);
					CURRGAME->name = strdup(p);
				} else  {
					if(!strcmp(p, "(M)MasterCode"))
						CURRGAME->n_items--;
					else
						CURRITEM->desc = strdup(p);
				}
				break;
			case TOK_NOTE:
				FETCH();
				CURRITEM->note = strdup(p);
				break;
			case TOK_GAMEID:
				assert(ts.sp == 3 && ts.stack[1] == TOK_GAME);
				FETCH();
				assert(strlen(p) == 13);
				memcpy(CURRGAME->id, p, 4);
				CURRGAME->crc = strtol(p+5, 0, 16);
				break;
			case TOK_CODES:
				assert(ts.sp > 2 && ts.stack[ts.sp-2] == TOK_CHEAT);
				FETCH();
				if(!strncmp(p, "master ", 7)) {
					p += 7;
					uint32_t m[8];
					for(i=0; i<8; ++i) {
						while(!isspace(*p)) ++p;
						if(!*p) {
							*error = "unexpected end of master key";
							return 0;
						}
						m[i] = strtol(++p, 0, 16);
						p += 9;
					}
					memcpy(CURRGAME->mastercode, m, 8*4);
					break;
				} else if(!strncmp(p, "on ", 3)) {
					CURRITEM->flags |= CHEAT_FLAG_ENABLED;
					p += 3;
				}
				uint32_t codes[sizeof(linebuf)/9];
				for(i=0; i<sizeof(codes)/4; ++i) {
					if(!ishexchar(p)) break;
					codes[i] = strtol(p, 0, 16);
					p += 8;
					if(!isspace(*p)) { ++i; break; }
					++p;
				}
				if(i >= sizeof(codes)/4) {
					*error = "line length max exceeded";
					return 0;
				}
				if(i & 1) {
					*error = "odd number of cheat words";
					return 0;
				}
				i /= 2;
				CURRITEM->codes = calloc(i, sizeof(struct archeat));
				CURRITEM->n = i;
				for(j = 0; j < i; ++j) {
					CURRITEM->codes[j].off = codes[j*2];
					CURRITEM->codes[j].val = codes[j*2+1];
				}
				break;
			default:
				assert(0);
			}
			static const int same_line_end_tokens[] = {
				TOK_NAME, TOK_NOTE, TOK_CODES, TOK_GAMEID, TOK_ALLOWEDON, TOK_DATE
			};
			for(i=0; i<sizeof(same_line_end_tokens)/4; ++i) {
				if(tok == same_line_end_tokens[i]) {
					p = q;
					if(*(p++) != '/') {
						*error = "expected '/'";
						return 0;
					}
					q = p;
					while(*q && *q != '>') ++q;
					if(*q != '>') goto e_gt;
					*q = 0;
					if(tok != get_xml_token(p)) goto e_mmc;
					tokstack_pop(&ts);
				}
			}
		}
	}
	return 1;
}


static int usage(char *argv0) {
	fprintf(stderr,
		"usrcheat " VERSION " (c) rofl0r\n\n"
		"a tool to translate r4/dsone usrcheat.dat files into cheats.xml format\n"
		"and vice versa.\n"
		"the xml format allows one to easily edit the cheat format with a text editor\n"
		"rather than a clumsy gui app, and it also allows automated updates.\n"
		"\n"
		"usage: %s MODE FILEIN FILEOUT\n"
		"MODE can be either of:\n"
		"toxml - read FILEIN as usrcheat.dat format, write FILEOUT as xml\n"
		"todat - read FILEIN as cheat.xml format, write FILEOUT as usrcheat.dat\n"
	, argv0);
	return 1;
}

int main(int argc, char** argv) {
	struct cheatdb db;
	char *error, *fnin, *fnout;
	if(argc != 4) return usage(argv[0]);
	int mode = -1;
	if(!strcmp(argv[1], "toxml")) mode = 0;
	if(!strcmp(argv[1], "todat")) mode = 1;
	if(mode == -1) return usage(argv[0]);
	fnin = argv[2];
	fnout = argv[3];
	if(mode == 0) {
		if(!cheatdb_read(&db, fnin, &error)) {
		e:
			fprintf(stderr, "error! %s\n", error);
			return 1;
		}
		if(!cheatdb_write_xml(&db, fnout, &error)) goto e;
	} else {
		long lineno;
		if(!cheatdb_read_xml(&db, fnin, &error, &lineno)) {
			fprintf(stderr, "error! %s at line %ld\n", error, lineno);
			return 1;
		}
		if(!cheatdb_write(&db, fnout, &error)) goto e;
	}
	return 0;
}
