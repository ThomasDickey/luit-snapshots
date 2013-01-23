/*
 * $XTermId: luitconv.h,v 1.12 2013/01/21 22:49:19 tom Exp $
 *
 * Copyright 2010,2013 by Thomas E. Dickey
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
#ifndef LUITCONV_H
#define LUITCONV_H

#ifdef USE_ICONV

typedef struct _FontMap {
    int type;
    unsigned (*recode) (unsigned, void *);	/* mapping function */
    void *client_data;          /* second parameter of the two above */
    struct _FontMap *next;      /* link to next element in list */
} FontMapRec, *FontMapPtr;

typedef struct _FontMapReverse {
    unsigned int (*reverse) (unsigned, void *);
    void *data;
} FontMapReverseRec, *FontMapReversePtr;

extern FontMapPtr luitLookupMapping(const char *);
extern unsigned luitMapCodeValue(unsigned, FontMapPtr);
extern FontMapReversePtr luitLookupReverse(FontMapPtr);

#ifdef NO_LEAKS
extern void luitDestroyReverse(FontMapReversePtr);
#endif

#define LookupMapping(encoding_name) \
	luitLookupMapping(encoding_name)

#define LookupReverse(fontmap_ptr) \
	luitLookupReverse(fontmap_ptr)

#define MapCodeValue(code, fontmap_ptr) \
	luitMapCodeValue(code, fontmap_ptr)

#else

#include <X11/fonts/fontenc.h>

#define LookupMapping(encoding_name) \
	FontEncMapFind(encoding_name, FONT_ENCODING_UNICODE, -1, -1, NULL)

#define LookupReverse(fontmap_ptr) \
	FontMapReverse(fontmap_ptr)

#define MapCodeValue(code, fontmap_ptr) \
	FontEncRecode(code, fontmap_ptr)

#endif

extern void reportFontencCharsets(void);
extern void reportIconvCharsets(void);
extern void showFontencCharset(const char *);

#endif /* LUITCONV_H */
