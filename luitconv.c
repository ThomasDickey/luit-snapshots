/*
 * $XTermId: luitconv.c,v 1.24 2010/11/28 20:54:51 tom Exp $
 *
 * Copyright 2010 by Thomas E. Dickey
 *
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of the above listed
 * copyright holder(s) not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.
 *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <other.h>

#include <iconv.h>

#include <sys.h>

/*
 * This uses a similar approach to vile's support for wide/narrow locales.
 *
 * We use the iconv library to construct a mapping forward for each byte,
 * into UTF-8, and then use that mapping to construct the reverse mapping
 * from UTF-8 into the original set of single byte values.
 *
 * This only works for 8-bit encodings, since iconv's API has insufficient
 * information to tell how many characters may be in an encoding.  We can
 * detect incomplete mappings, which hint at multibyte encodings.
 */

#define UCS_REPL        0xfffd
#define DEC_REPL	0x2426	/* SYMBOL FOR SUBSTITUTE FORM TWO */

#define MAX8		256

#define NO_ICONV  (iconv_t)(-1)

/******************************************************************************/
typedef struct {
    size_t size;		/* length of text[] */
    char *text;			/* value, in UTF-8 */
    unsigned ucs;		/* corresponding Unicode value */
} MappingData;

typedef struct {
    unsigned ucs;
    unsigned ch;
} ReverseData;

typedef struct _LuitConv {
    struct _LuitConv *next;
    char *encoding_name;
    iconv_t iconv_desc;
    /* internal tables for input/output */
    MappingData table_utf8[MAX8];	/* UTF-8 equivalents of 8-bit codes */
    ReverseData rev_index[MAX8];	/* reverse-index */
    size_t len_index;		/* index length */
    /* data expected by caller */
    FontMapRec mapping;
    FontMapReverseRec reverse;
} LuitConv;

static LuitConv *all_conversions;

/******************************************************************************/

typedef struct {
    unsigned source;
    unsigned target;
} BuiltInMapping;

typedef struct _BuiltInCharset {
    const char *name;		/* table name, for lookups */
    int type;			/* FIXME - to do */
    const BuiltInMapping *table;
    size_t length;		/* length of table[] */
    unsigned shift;		/* FIXME - to-do */
    unsigned fill;		/* value to use for undefined characters */
} BuiltInCharsetRec;

/* originally derived from the file data.c in the XTerm sources */
static const BuiltInMapping dec_special[] =
{
    {0x5f, 0x25ae},		/* black vertical rectangle */
    {0x60, 0x25c6},		/* black diamond */
    {0x61, 0x2592},		/* medium shade */
    {0x62, 0x2409},		/* symbol for horizontal tabulation */
    {0x63, 0x240c},		/* symbol for form feed */
    {0x64, 0x240d},		/* symbol for carriage return */
    {0x65, 0x240a},		/* symbol for line feed */
    {0x66, 0x00b0},		/* degree sign */
    {0x67, 0x00b1},		/* plus-minus sign */
    {0x68, 0x2424},		/* symbol for newline */
    {0x69, 0x240b},		/* symbol for vertical tabulation */
    {0x6a, 0x2518},		/* box drawings light up and left */
    {0x6b, 0x2510},		/* box drawings light down and left */
    {0x6c, 0x250c},		/* box drawings light down and right */
    {0x6d, 0x2514},		/* box drawings light up and right */
    {0x6e, 0x253c},		/* box drawings light vertical and horizontal */
    {0x6f, 0x23ba},		/* box drawings scan 1 */
    {0x70, 0x23bb},		/* box drawings scan 3 */
    {0x71, 0x2500},		/* box drawings light horizontal */
    {0x72, 0x23bc},		/* box drawings scan 7 */
    {0x73, 0x23bd},		/* box drawings scan 9 */
    {0x74, 0x251c},		/* box drawings light vertical and right */
    {0x75, 0x2524},		/* box drawings light vertical and left */
    {0x76, 0x2534},		/* box drawings light up and horizontal */
    {0x77, 0x252c},		/* box drawings light down and horizontal */
    {0x78, 0x2502},		/* box drawings light vertical */
    {0x79, 0x2264},		/* less-than or equal to */
    {0x7a, 0x2265},		/* greater-than or equal to */
    {0x7b, 0x03c0},		/* greek small letter pi */
    {0x7c, 0x2260},		/* not equal to */
    {0x7d, 0x00a3},		/* pound sign */
    {0x7e, 0x00b7},		/* middle dot */
};

/* derived from http://www.vt100.net/charsets/technical.html */
static const BuiltInMapping dec_dectech[] =
{
    {0x21, 0x23b7},		/* RADICAL SYMBOL BOTTOM Centred left to right, so that it joins up with 02/02 */
    {0x22, 0x250c},		/* BOX DRAWINGS LIGHT DOWN AND RIGHT */
    {0x23, 0x2500},		/* BOX DRAWINGS LIGHT HORIZONTAL */
    {0x24, 0x2320},		/* TOP HALF INTEGRAL with the proviso that the stem is vertical, to join with 02/06 */
    {0x25, 0x2321},		/* BOTTOM HALF INTEGRAL with the proviso above. */
    {0x26, 0x2502},		/* BOX DRAWINGS LIGHT VERTICAL */
    {0x27, 0x23a1},		/* LEFT SQUARE BRACKET UPPER CORNER Joins vertically to 02/06, 02/08. Doesn't join to its right. */
    {0x28, 0x23a3},		/* LEFT SQUARE BRACKET LOWER CORNER Joins vertically to 02/06, 02/07. Doesn't join to its right. */
    {0x29, 0x23a4},		/* RIGHT SQUARE BRACKET UPPER CORNER Joins vertically to 026, 02a. Doesn't join to its left. */
    {0x2a, 0x23a6},		/* RIGHT SQUARE BRACKET LOWER CORNER Joins vertically to 026, 029. Doesn't join to its left. */
    {0x2b, 0x239b},		/* LEFT PARENTHESIS UPPER HOOK Joins vertically to 026, 02c, 02/15. Doesn't join to its right. */
    {0x2c, 0x239d},		/* LEFT PARENTHESIS LOWER HOOK Joins vertically to 026, 02b, 02/15. Doesn't join to its right. */
    {0x2d, 0x239e},		/* RIGHT PARENTHESIS UPPER HOOK Joins vertically to 026, 02e, 03/00. Doesn't join to its left. */
    {0x2e, 0x23a0},		/* RIGHT PARENTHESIS LOWER HOOK Joins vertically to 026, 02d, 03/00. Doesn't join to its left. */
    {0x2f, 0x23a8},		/* LEFT CURLY BRACKET MIDDLE PIECE Joins vertically to 026, 02b, 02c. */
    {0x30, 0x23ac},		/* RIGHT CURLY BRACKET MIDDLE PIECE Joins vertically to 02/06, 02d, 02e. */
    {0x31, DEC_REPL},		/* Top Left Sigma. Joins to right with 02/03, 03/05. Joins diagonally below right with 03/03, 03/07. */
    {0x32, DEC_REPL},		/* Bottom Left Sigma. Joins to right with 02/03, 03/06. Joins diagonally above right with 03/04, 03/07. */
    {0x33, DEC_REPL},		/* Top Diagonal Sigma. Line for joining 03/01 to 03/04 or 03/07. */
    {0x34, DEC_REPL},		/* Bottom Diagonal Sigma. Line for joining 03/02 to 03/03 or 03/07. */
    {0x35, DEC_REPL},		/* Top Right Sigma. Joins to left with 02/03, 03/01. */
    {0x36, DEC_REPL},		/* Bottom Right Sigma. Joins to left with 02/03, 03/02. */
    {0x37, DEC_REPL},		/* Middle Sigma. Joins diagonally with 03/01, 03/02, 03/03, 03/04. */
    {0x38, DEC_REPL},		/* undefined */
    {0x39, DEC_REPL},		/* undefined */
    {0x3a, DEC_REPL},		/* undefined */
    {0x3b, DEC_REPL},		/* undefined */
    {0x3c, 0x2264},		/* LESS-THAN OR EQUAL TO */
    {0x3d, 0x2260},		/* NOT EQUAL TO */
    {0x3e, 0x2265},		/* GREATER-THAN OR EQUAL TO */
    {0x3f, 0x222B},		/* INTEGRAL */
    {0x40, 0x2234},		/* THEREFORE */
    {0x41, 0x221d},		/* PROPORTIONAL TO */
    {0x42, 0x221e},		/* INFINITY */
    {0x43, 0x00f7},		/* DIVISION SIGN */
    {0x44, 0x039a},		/* GREEK CAPITAL DELTA */
    {0x45, 0x2207},		/* NABLA */
    {0x46, 0x03a6},		/* GREEK CAPITAL LETTER PHI */
    {0x47, 0x0393},		/* GREEK CAPITAL LETTER GAMMA */
    {0x48, 0x223c},		/* TILDE OPERATOR */
    {0x49, 0x2243},		/* ASYMPTOTICALLY EQUAL TO */
    {0x4a, 0x0398},		/* GREEK CAPITAL LETTER THETA */
    {0x4b, 0x00d7},		/* MULTIPLICATION SIGN */
    {0x4c, 0x039b},		/* GREEK CAPITAL LETTER LAMDA */
    {0x4d, 0x21d4},		/* LEFT RIGHT DOUBLE ARROW */
    {0x4e, 0x21d2},		/* RIGHTWARDS DOUBLE ARROW */
    {0x4f, 0x2261},		/* IDENTICAL TO */
    {0x50, 0x03a0},		/* GREEK CAPITAL LETTER PI */
    {0x51, 0x03a8},		/* GREEK CAPITAL LETTER PSI */
    {0x52, DEC_REPL},		/* undefined */
    {0x53, 0x03a3},		/* GREEK CAPITAL LETTER SIGMA */
    {0x54, DEC_REPL},		/* undefined */
    {0x55, DEC_REPL},		/* undefined */
    {0x56, 0x221a},		/* SQUARE ROOT */
    {0x57, 0x03a9},		/* GREEK CAPITAL LETTER OMEGA */
    {0x58, 0x039e},		/* GREEK CAPITAL LETTER XI */
    {0x59, 0x03a5},		/* GREEK CAPITAL LETTER UPSILON */
    {0x5a, 0x2282},		/* SUBSET OF */
    {0x5b, 0x2283},		/* SUPERSET OF */
    {0x5c, 0x2229},		/* INTERSECTION */
    {0x5d, 0x222a},		/* UNION */
    {0x5e, 0x2227},		/* LOGICAL AND */
    {0x5f, 0x2228},		/* LOGICAL OR */
    {0x60, 0x00ac},		/* NOT SIGN */
    {0x61, 0x03b1},		/* GREEK SMALL LETTER ALPHA */
    {0x62, 0x03b2},		/* GREEK SMALL LETTER BETA */
    {0x63, 0x03c7},		/* GREEK SMALL LETTER CHI */
    {0x64, 0x03b4},		/* GREEK SMALL LETTER DELTA */
    {0x65, 0x03b5},		/* GREEK SMALL LETTER EPSILON */
    {0x66, 0x03c6},		/* GREEK SMALL LETTER PHI */
    {0x67, 0x03b3},		/* GREEK SMALL LETTER GAMMA */
    {0x68, 0x03b7},		/* GREEK SMALL LETTER ETA */
    {0x69, 0x03b9},		/* GREEK SMALL LETTER IOTA */
    {0x6a, 0x03b8},		/* GREEK SMALL LETTER THETA */
    {0x6b, 0x03ba},		/* GREEK SMALL LETTER KAPPA */
    {0x6c, 0x03bb},		/* GREEK SMALL LETTER LAMDA */
    {0x6d, DEC_REPL},		/* undefined */
    {0x6e, 0x03bd},		/* GREEK SMALL LETTER NU */
    {0x6f, 0x2202},		/* PARTIAL DIFFERENTIAL */
    {0x70, 0x03c0},		/* GREEK SMALL LETTER PI */
    {0x71, 0x03c8},		/* GREEK SMALL LETTER PSI */
    {0x72, 0x03c1},		/* GREEK SMALL LETTER RHO */
    {0x73, 0x03c3},		/* GREEK SMALL LETTER SIGMA */
    {0x74, 0x03c4},		/* GREEK SMALL LETTER TAU */
    {0x75, DEC_REPL},		/* undefined */
    {0x76, 0x0192},		/* LATIN SMALL LETTER F WITH HOOK Probably chosen for its meaning of "function" */
    {0x77, 0x03c9},		/* GREEK SMALL LETTER OMEGA */
    {0x78, 0x03bE},		/* GREEK SMALL LETTER XI */
    {0x79, 0x03c5},		/* GREEK SMALL LETTER UPSILON */
    {0x7a, 0x03b6},		/* GREEK SMALL LETTER ZETA */
    {0x7b, 0x2190},		/* LEFTWARDS ARROW */
    {0x7c, 0x2191},		/* UPWARDS ARROW */
    {0x7d, 0x2192},		/* RIGHTWARDS ARROW */
    {0x7e, 0x2193},		/* DOWNWARDS ARROW */
};

/******************************************************************************/
static int
ConvToUTF32(unsigned *target, const char *source, size_t limit)
{
#define CH(n) (UCHAR)((*target) >> ((n) * 8))
    int rc = 0;
    int j;
    UINT mask = 0;

    /*
     * Find the number of bytes we will need from the source.
     */
    if ((*source & 0x80) == 0) {
	rc = 1;
	mask = (UINT) * source;
    } else if ((*source & 0xe0) == 0xc0) {
	rc = 2;
	mask = (UINT) (*source & 0x1f);
    } else if ((*source & 0xf0) == 0xe0) {
	rc = 3;
	mask = (UINT) (*source & 0x0f);
    } else if ((*source & 0xf8) == 0xf0) {
	rc = 4;
	mask = (UINT) (*source & 0x07);
    } else if ((*source & 0xfc) == 0xf8) {
	rc = 5;
	mask = (UINT) (*source & 0x03);
    } else if ((*source & 0xfe) == 0xfc) {
	rc = 6;
	mask = (UINT) (*source & 0x01);
    }

    if ((size_t) rc > limit) {	/* whatever it is, we cannot decode it */
	TRACE2(("limit failed %d/%ld in vl_conv_to_utf32\n", rc, limit));
	rc = 0;
    }

    /*
     * sanity-check.
     */
    if (rc > 1) {
	for (j = 1; j < rc; j++) {
	    if ((source[j] & 0xc0) != 0x80)
		break;
	}
	if (j != rc) {
	    TRACE2(("check failed %d/%d in vl_conv_to_utf32\n", j, rc));
	    rc = 0;
	}
    }

    if (target != 0) {
	int shift = 0;
	*target = 0;
	for (j = 1; j < rc; j++) {
	    *target |= (UINT) (source[rc - j] & 0x3f) << shift;
	    shift += 6;
	}
	*target |= mask << shift;

	TRACE2(("encode %2d:%.*s -> %#08x %02X.%02X.%02X.%02X\n",
		rc, rc, source,
		*target,
		CH(3), CH(2), CH(1), CH(0)));
    }
    return rc;
#undef CH
}

static int
ConvToUTF8(UCHAR * target, UINT source, size_t limit)
{
#define CH(n) (UCHAR)((source) >> ((n) * 8))
    int rc = 0;

    if (source <= 0x0000007f)
	rc = 1;
    else if (source <= 0x000007ff)
	rc = 2;
    else if (source <= 0x0000ffff)
	rc = 3;
    else if (source <= 0x001fffff)
	rc = 4;
    else if (source <= 0x03ffffff)
	rc = 5;
    else			/* (source <= 0x7fffffff) */
	rc = 6;

    if ((size_t) rc > limit) {	/* whatever it is, we cannot decode it */
	TRACE2(("limit failed in vl_conv_to_utf8 %d/%ld %#06x\n",
		rc, limit, source));
	rc = 0;
    }

    if (target != 0) {
	switch (rc) {
	case 1:
	    target[0] = (UCHAR) CH(0);
	    break;

	case 2:
	    target[1] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[0] = (UCHAR) (0xc0 | (CH(0) >> 6) | ((CH(1) & 0x07) << 2));
	    break;

	case 3:
	    target[2] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[1] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[0] = (UCHAR) (0xe0 | ((int) (CH(1) & 0xf0) >> 4));
	    break;

	case 4:
	    target[3] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[2] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[1] = (UCHAR) (0x80 |
				 ((int) (CH(1) & 0xf0) >> 4) |
				 ((int) (CH(2) & 0x03) << 4));
	    target[0] = (UCHAR) (0xf0 | ((int) (CH(2) & 0x1f) >> 2));
	    break;

	case 5:
	    target[4] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[3] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[2] = (UCHAR) (0x80 |
				 ((int) (CH(1) & 0xf0) >> 4) |
				 ((int) (CH(2) & 0x03) << 4));
	    target[1] = (UCHAR) (0x80 | (CH(2) >> 2));
	    target[0] = (UCHAR) (0xf8 | (CH(3) & 0x03));
	    break;

	case 6:
	    target[5] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[4] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[3] = (UCHAR) (0x80 | (CH(1) >> 4) | ((CH(2) & 0x03) << 4));
	    target[2] = (UCHAR) (0x80 | (CH(2) >> 2));
	    target[1] = (UCHAR) (0x80 | (CH(3) & 0x3f));
	    target[0] = (UCHAR) (0xfc | ((int) (CH(3) & 0x40) >> 6));
	    break;
	}
	TRACE2(("decode %#08x %02X.%02X.%02X.%02X %d:%.*s\n", source,
		CH(3), CH(2), CH(1), CH(0), rc, rc, target));
    }

    return rc;			/* number of bytes needed in target */
#undef CH
}

/******************************************************************************/

static int
cmp_rindex(const void *a, const void *b)
{
    const ReverseData *p = (const ReverseData *) a;
    const ReverseData *q = (const ReverseData *) b;
    return (int) (p)->ucs - (int) (q)->ucs;
}

static void
initializeIconvTable(LuitConv * data)
{
    unsigned n;

    data->len_index = 0;

    for (n = 0; n < MAX8; ++n) {
	size_t converted;
	char input[80];
	ICONV_CONST char *ip = input;
	char output[80];
	char *op = output;
	size_t in_bytes = 1;
	size_t out_bytes = sizeof(output);

	input[0] = (char) n;
	input[1] = 0;
	converted = iconv(data->iconv_desc, &ip, &in_bytes, &op, &out_bytes);
	if (converted == (size_t) (-1)) {
	    TRACE(("convert err %d\n", n));
	    data->table_utf8[n].ucs = UCS_REPL;
	} else {
	    output[sizeof(output) - out_bytes] = 0;
	    data->table_utf8[n].size = sizeof(output) - out_bytes;
	    data->table_utf8[n].text = malloc(data->table_utf8[n].size);
	    memcpy(data->table_utf8[n].text,
		   output,
		   data->table_utf8[n].size);
	    if (ConvToUTF32((UINT *) 0,
			    data->table_utf8[n].text,
			    data->table_utf8[n].size)) {
		ConvToUTF32(&(data->table_utf8[n].ucs),
			    data->table_utf8[n].text,
			    data->table_utf8[n].size);
	    } else {
		data->table_utf8[n].ucs = UCS_REPL;
	    }
	    TRACE(("convert %4X:%d:%04X:%.*s\n", n,
		   (int) data->table_utf8[n].size,
		   data->table_utf8[n].ucs,
		   (int) data->table_utf8[n].size,
		   data->table_utf8[n].text));

	    data->rev_index[data->len_index].ucs = data->table_utf8[n].ucs;
	    data->rev_index[data->len_index].ch = n;
	    data->len_index++;
	}
    }
}

static unsigned
luitRecode(unsigned code, void *client_data GCC_UNUSED)
{
    TRACE(("luitRecode 0x%04X %p\n", code, client_data));
    return code;
}

static unsigned
luitReverse(unsigned code, void *client_data GCC_UNUSED)
{
    unsigned result = code;
    LuitConv *data = (LuitConv *) client_data;

    TRACE(("luitReverse 0x%04X %p\n", code, data));

    if (data != 0) {
	ReverseData *p;
	ReverseData key;

	key.ucs = (UINT) code;
	p = (ReverseData *) bsearch(&key,
				    data->rev_index,
				    data->len_index,
				    sizeof(data->rev_index[0]),
				    cmp_rindex);

	if (p != 0) {
	    result = p->ch;
	    TRACE(("...mapped %#x\n", result));
	}
    }
    return result;
}

/*
 * Translate one of luit's encoding names to one which is more likely to
 * work with iconv.
 */
static const char *
findEncodingAlias(const char *encoding_name)
{
    /* *INDENT-OFF* */
    static const struct {
	const char *luit_name;
	const char *iconv_name;
    } table[] = {
	{ "KOI8-E",		"ISO-IR-111" },
	{ "TCVN-0",		"TCVN5712-1:1993" },
	{ "ibm-cp437",		"cp437" },
	{ "ibm-cp850",		"cp850" },
	{ "ibm-cp866",		"cp866" },
	{ "iso646.1973-0",	"US-ASCII" },
	{ "microsoft-cp1250",   "windows-1250" },
	{ "microsoft-cp1251",   "windows-1251" },
	{ "microsoft-cp1252",   "windows-1252" },
#if 0	/* FIXME - not 8-bit character sets */
	{ "big5.eten-0",	"BIG-5" },
	{ "ksc5601.1987-0",	"JOHAB" },
	{ "gb2312.1980-0",	"GB2312" },
#endif
    };
    /* *INDENT-ON* */

    size_t n;
    const char *result = 0;

    for (n = 0; n < SizeOf(table); ++n) {
	if (!strcasecmp(encoding_name, table[n].luit_name)) {
	    result = table[n].iconv_name;
	    break;
	}
    }
    return result;
}

static void
initializeBuiltInTable(LuitConv * data, const BuiltInCharsetRec * builtIn)
{
    UCHAR buffer[20];
    size_t n;
    size_t need;

    TRACE(("initializing built-in '%s'\n", builtIn->name));

    data->len_index = 0;

    for (n = 0; n < MAX8; ++n) {
	data->table_utf8[n].ucs = (builtIn->fill
				   ? builtIn->fill
				   : (unsigned) n);
    }

    for (n = 0; n < builtIn->length; ++n) {
	if (builtIn->table[n].source < MAX8) {
	    size_t j = builtIn->table[n].source;

	    data->table_utf8[j].ucs = builtIn->table[n].target;

	    if ((need = (size_t) ConvToUTF8(buffer,
					    data->table_utf8[j].ucs,
					    sizeof(buffer) - 1)) != 0) {
		data->table_utf8[j].text = malloc(need + 1);
		data->table_utf8[j].size = need;
		memcpy(data->table_utf8[j].text, buffer, need);
	    }

	    TRACE(("convert %4X:%d:%04X:%.*s\n", j,
		   (int) data->table_utf8[j].size,
		   data->table_utf8[j].ucs,
		   (int) data->table_utf8[j].size,
		   data->table_utf8[j].text));

	    data->rev_index[data->len_index].ucs = data->table_utf8[j].ucs;
	    data->rev_index[data->len_index].ch = (unsigned) j;
	    data->len_index++;
	}
    }
}

static const BuiltInCharsetRec *
findBuiltinEncoding(const char *encoding_name)
{
    static const BuiltInCharsetRec table[] =
    {
	{"dec-special", 0, dec_special, SizeOf(dec_special), 0, 0},
	{"dec-dectech", 0, dec_dectech, SizeOf(dec_dectech), 0, DEC_REPL},
    };
    size_t n;
    const BuiltInCharsetRec *result = 0;

    for (n = 0; n < SizeOf(table); ++n) {
	if (!strcasecmp(encoding_name, table[n].name)) {
	    result = &(table[n]);
	    break;
	}
    }

    return result;
}

/******************************************************************************/
FontMapPtr
luitLookupMapping(const char *encoding_name)
{
    FontMapPtr result = 0;
    LuitConv *latest = 0;
    const BuiltInCharsetRec *builtIn;
    iconv_t my_desc;

    TRACE(("luitLookupMapping '%s'\n", encoding_name));

    for (latest = all_conversions; latest != 0; latest = latest->next) {
	if (!strcasecmp(encoding_name, latest->encoding_name)) {
	    TRACE(("...found mapping in cache\n"));
	    result = &(latest->mapping);
	    break;
	}
    }

    if (latest == 0) {
	my_desc = iconv_open("UTF-8", encoding_name);
	if (my_desc == NO_ICONV) {
	    const char *alias = findEncodingAlias(encoding_name);
	    if (alias != 0) {
		encoding_name = alias;
		TRACE(("...retry '%s'\n", encoding_name));
		my_desc = iconv_open("UTF-8", encoding_name);
	    }
	}
	if (my_desc != NO_ICONV) {
	    TRACE(("...iconv_open succeeded\n"));
	    latest = TypeCalloc(LuitConv);
	    if (latest != 0) {
		latest->next = all_conversions;
		latest->encoding_name = strmalloc(encoding_name);
		latest->iconv_desc = my_desc;
		initializeIconvTable(latest);
		latest->mapping.recode = luitRecode;
		latest->reverse.reverse = luitReverse;
		latest->reverse.data = latest;
		all_conversions = latest;

		result = &(latest->mapping);
	    }
	} else if ((builtIn = findBuiltinEncoding(encoding_name)) != 0) {
	    TRACE(("...use built-in charset\n"));
	    latest = TypeCalloc(LuitConv);
	    if (latest != 0) {
		latest->next = all_conversions;
		latest->encoding_name = strmalloc(encoding_name);
		latest->iconv_desc = my_desc;
		initializeBuiltInTable(latest, builtIn);
		latest->mapping.recode = luitRecode;
		latest->reverse.reverse = luitReverse;
		latest->reverse.data = latest;
		all_conversions = latest;

		result = &(latest->mapping);
	    }
	}

	/* sort the reverse-index, to allow using bsearch */
	if (result != 0) {
	    qsort(latest->rev_index,
		  latest->len_index,
		  sizeof(latest->rev_index[0]),
		  cmp_rindex);
	}
    }

    TRACE(("... luitLookupMapping ->%p\n", result));
    return result;
}

FontMapReversePtr
luitLookupReverse(FontMapPtr fontmap_ptr)
{
    FontMapReversePtr result = 0;
    LuitConv *search;

    TRACE(("luitLookupReverse %p\n", fontmap_ptr));
    for (search = all_conversions; search != 0; search = search->next) {
	if (fontmap_ptr == &(search->mapping)) {
	    TRACE(("...found %s\n", search->encoding_name));
	    result = &(search->reverse);
	    break;
	}
    }
    return result;
}

unsigned
luitMapCodeValue(unsigned code, FontMapPtr fontmap_ptr)
{
    unsigned result;
    LuitConv *search;

    result = code;
    if (code < MAX8) {
	for (search = all_conversions; search != 0; search = search->next) {
	    if (&(search->mapping) == fontmap_ptr) {
		result = search->table_utf8[code].ucs;
		break;
	    }
	}
    }

    TRACE2(("luitMapCodeValue 0x%04X '%c' 0x%04X\n",
	    code,
	    isprint(code) ? code : ' ',
	    result));
    return result;
}

#ifdef NO_LEAKS
/*
 * Given a reverse-pointer, remove all of the corresponding cached information
 * from this module.
 */
void
luitDestroyReverse(FontMapReversePtr reverse)
{
    LuitConv *p, *q;
    int n;

    for (p = all_conversions, q = 0; p != 0; q = p, p = p->next) {
	if (&(p->reverse) == reverse) {

	    free(p->encoding_name);
	    iconv_close(p->iconv_desc);

	    for (n = 0; n < MAX8; ++n) {
		if (p->table_utf8[n].text) {
		    free(p->table_utf8[n].text);
		}
	    }

	    /* delink and destroy */
	    if (q != 0)
		q->next = p->next;
	    else
		all_conversions = p->next;
	    free(p);
	    break;
	}
    }
}
#endif
