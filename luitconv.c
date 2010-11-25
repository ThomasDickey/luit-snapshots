/*
 * $XTermId: luitconv.c,v 1.12 2010/11/25 01:41:43 tom Exp $
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <iconv.h>
#include <ctype.h>
#include <string.h>		/* strcasecmp */

#include "sys.h"
#include "other.h"

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

#define NO_ICONV  (iconv_t)(-1)

typedef struct {
    size_t size;		/* length of text[] */
    char *text;			/* value, in UTF-8 */
    unsigned ucs;		/* corresponding Unicode value */
} MappingData;

typedef struct _LuitConv {
    struct _LuitConv *next;
    char *encoding_name;
    iconv_t iconv_desc;
    /* UTF-8 equivalents of the 8-bit codes */
    MappingData table_utf8[256];
    FontMapRec mapping;
    FontMapReverseRec reverse;
} LuitConv;

static LuitConv *all_conversions;

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

static void
initialize_table_8bit_utf8(LuitConv * data)
{
    int n;

    for (n = 0; n < 256; ++n) {
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
	    data->table_utf8[n].text = strmalloc(output);
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
	}
    }
}

static unsigned
luitRecode(unsigned code, void *client_data)
{
    TRACE(("luitRecode 0x%04X %p\n", code, client_data));
    return code;
}

static unsigned
luitReverse(unsigned code, void *client_data)
{
    TRACE(("luitReverse 0x%04X %p\n", code, client_data));
    return code;
}

/*
 * Translate one of luit's encoding names to one which is more likely to
 * work with iconv.
 */
static const char *
findEncodingAlias(const char *encoding_name)
{
    /* *INDENT-OFF* */
    static struct {
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
#if 0	/* FIXME - not supported by iconv, but needed */
	{ "dec-special",        "?" },	/* vt100 */
	{ "dec-dectech",        "?" },	/* vt220 */
#endif
#if 0	/* FIXME - not 8-bit character sets */
	{ "big5.eten-0",	"BIG-5" },
	{ "ksc5601.1987-0",	"JOHAB" },
	{ "gb2312.1980-0",	"GB2312" },
#endif
    };
    /* *INDENT-ON* */

    size_t n;
    const char *result = 0;

    for (n = 0; n < sizeof(table) / sizeof(table[0]); ++n) {
	if (!strcasecmp(encoding_name, table[n].luit_name)) {
	    result = table[n].iconv_name;
	    break;
	}
    }
    return result;
}

FontMapPtr
luitLookupMapping(const char *encoding_name)
{
    FontMapPtr result = 0;
    LuitConv *latest = 0;
    iconv_t my_desc;

    TRACE(("luitLookupMapping '%s'\n", encoding_name));

    for (latest = all_conversions; latest != 0; latest = latest->next) {
	if (!strcasecmp(encoding_name, latest->encoding_name)) {
	    TRACE(("...found mapping in cache\n"));
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
		initialize_table_8bit_utf8(latest);
		latest->mapping.recode = luitRecode;
		latest->reverse.reverse = luitReverse;
		all_conversions = latest;

		result = &(latest->mapping);
	    }
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

    TRACE(("luitMapCodeValue 0x%04X '%c' %p\n",
	   code,
	   isprint(code) ? code : ' ',
	   fontmap_ptr));

    result = code;
    if (code < 256) {
	for (search = all_conversions; search != 0; search = search->next) {
	    if (&(search->mapping) == fontmap_ptr) {
		result = search->table_utf8[code].ucs;
		break;
	    }
	}
    }
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

	    for (n = 0; n < 256; ++n) {
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
