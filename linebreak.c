#include "linebreak.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hb.h>
#include <hb-ft.h>
#include <assert.h>
#include "unicodeTables.h"

enum UnicodeType getUnicodeType(int codepoint) {
	return typePropertyTable[codepoint >> 8] >= UNICODE_MAX_TYPE_TABLE_INDEX
		? typePropertyTable[codepoint >> 8] - UNICODE_MAX_TYPE_TABLE_INDEX
		: typePropertyData[typePropertyTable[codepoint >> 8]][codepoint & 0xFF];
}

enum BreakClass getBreakClass(int codepoint) {
	return breakPropertyTable[codepoint >> 8] >= UNICODE_MAX_BREAK_TABLE_INDEX
		? breakPropertyTable[codepoint >> 8] - UNICODE_MAX_BREAK_TABLE_INDEX
		: breakPropertyData[breakPropertyTable[codepoint >> 8]][codepoint & 0xFF];
}

enum {
	INDEX_OPEN_PUNCTUATION,
	INDEX_CLOSE_PUNCTUATION,
	INDEX_CLOSE_PARENTHESIS,
	INDEX_QUOTATION,
	INDEX_NON_BREAKING_GLUE,
	INDEX_NON_STARTER,
	INDEX_EXCLAMATION,
	INDEX_SYMBOL,
	INDEX_INFIX_SEPARATOR,
	INDEX_PREFIX,
	INDEX_POSTFIX,
	INDEX_NUMERIC,
	INDEX_ALPHABETIC,
	INDEX_HEBREW_LETTER,
	INDEX_IDEOGRAPHIC,
	INDEX_INSEPARABLE,
	INDEX_HYPHEN,
	INDEX_AFTER,
	INDEX_BEFORE,
	INDEX_BEFORE_AND_AFTER,
	INDEX_ZERO_WIDTH_SPACE,
	INDEX_COMBINING_MARK,
	INDEX_WORD_JOINER,
	INDEX_HANGUL_LV_SYLLABLE,
	INDEX_HANGUL_LVT_SYLLABLE,
	INDEX_HANGUL_L_JAMO,
	INDEX_HANGUL_V_JAMO,
	INDEX_HANGUL_T_JAMO,
	INDEX_REGIONAL_INDICATOR,
	INDEX_EMOJI_BASE,
	INDEX_EMOJI_MODIFIER,
	INDEX_ZERO_WIDTH_JOINER,
	/* End of the table */
	INDEX_END_OF_TABLE,
	/* The following are not in the tables */
	INDEX_MANDATORY,
	INDEX_CARRIAGE_RETURN,
	INDEX_LINE_FEED,
	INDEX_SURROGATE,
	INDEX_CONTINGENT,
	INDEX_SPACE,
	INDEX_COMPLEX_CONTEXT,
	INDEX_AMBIGUOUS,
	INDEX_UNKNOWN,
	INDEX_NEXT_LINE,
	INDEX_CONDITIONAL_JAPANESE_STARTER,
};

static const enum BreakAction breakPairs[INDEX_END_OF_TABLE][INDEX_END_OF_TABLE] = {
	{PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, COMBINING_PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK},
	{DIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, PROHIBITED_BREAK, COMBINING_INDIRECT_BREAK, PROHIBITED_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, DIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK, INDIRECT_BREAK}
};

/* Map GBreakClass to table indices */
static const int lineBreakIndices[] = {
	INDEX_MANDATORY,
	INDEX_CARRIAGE_RETURN,
	INDEX_LINE_FEED,
	INDEX_COMBINING_MARK,
	INDEX_SURROGATE,
	INDEX_ZERO_WIDTH_SPACE,
	INDEX_INSEPARABLE,
	INDEX_NON_BREAKING_GLUE,
	INDEX_CONTINGENT,
	INDEX_SPACE,
	INDEX_AFTER,
	INDEX_BEFORE,
	INDEX_BEFORE_AND_AFTER,
	INDEX_HYPHEN,
	INDEX_NON_STARTER,
	INDEX_OPEN_PUNCTUATION,
	INDEX_CLOSE_PUNCTUATION,
	INDEX_QUOTATION,
	INDEX_EXCLAMATION,
	INDEX_IDEOGRAPHIC,
	INDEX_NUMERIC,
	INDEX_INFIX_SEPARATOR,
	INDEX_SYMBOL,
	INDEX_ALPHABETIC,
	INDEX_PREFIX,
	INDEX_POSTFIX,
	INDEX_COMPLEX_CONTEXT,
	INDEX_AMBIGUOUS,
	INDEX_UNKNOWN,
	INDEX_NEXT_LINE,
	INDEX_WORD_JOINER,
	INDEX_HANGUL_L_JAMO,
	INDEX_HANGUL_V_JAMO,
	INDEX_HANGUL_T_JAMO,
	INDEX_HANGUL_LV_SYLLABLE,
	INDEX_HANGUL_LVT_SYLLABLE,
	INDEX_CLOSE_PARENTHESIS,
	INDEX_CONDITIONAL_JAPANESE_STARTER,
	INDEX_HEBREW_LETTER,
	INDEX_REGIONAL_INDICATOR,
	INDEX_EMOJI_BASE,
	INDEX_EMOJI_MODIFIER,
	INDEX_ZERO_WIDTH_JOINER
};

int findLineBreak(enum BreakClass *pcls, enum BreakAction *pbrk, int length) {
	enum BreakClass cls = pcls[0]; // Class of 'before' character
	// Treat SP at start of input as if it followed a WJ
	if (cls == UNICODE_BREAK_SPACE) cls = UNICODE_BREAK_WORD_JOINER;
	// Handle case where input starts with an LF and treat initial NL like BK
	if (cls == UNICODE_BREAK_LINE_FEED || cls == UNICODE_BREAK_NEXT_LINE) cls = UNICODE_BREAK_MANDATORY;

	// Loop over all pairs in the string up to a hard break or CRLF pair
	int i = 1;
	for (; i < length && cls != UNICODE_BREAK_MANDATORY
			&& (cls != UNICODE_BREAK_CARRIAGE_RETURN || pcls[i] == UNICODE_BREAK_LINE_FEED); ++i) {
		// Handle BK, NL and LF explicitly
		if (pcls[i] == UNICODE_BREAK_MANDATORY || pcls[i] == UNICODE_BREAK_NEXT_LINE || pcls[i] == UNICODE_BREAK_LINE_FEED) {
			pbrk[i - 1] = PROHIBITED_BREAK;
			cls = UNICODE_BREAK_MANDATORY;
			continue;
		}
		// Handle CR explicitly
		if(pcls[i] == UNICODE_BREAK_CARRIAGE_RETURN) {
			pbrk[i - 1] = PROHIBITED_BREAK;
			cls = UNICODE_BREAK_CARRIAGE_RETURN;
			continue;
		}
		// Handle spaces explicitly
		if (pcls[i] == UNICODE_BREAK_SPACE) {
			pbrk[i - 1] = PROHIBITED_BREAK; // Apply rule LB7: × SP
			continue; // Do not update cls
		}
		// Handle complex scripts
		if (pcls[i] == UNICODE_BREAK_COMPLEX_CONTEXT) {
			assert(0 && "Complex scripts are unsupported.");
		}

		// Lookup pair table information in breakPairs[before][after];
		int first = lineBreakIndices[cls], second = lineBreakIndices[pcls[i]];
		assert(first < INDEX_END_OF_TABLE && second < INDEX_END_OF_TABLE && "Index out of bounds.");
		enum BreakAction brk = breakPairs[first][second];

		// Resolve indirect break and handle breaks involving a combining mark (see Section 7.5)
		switch (brk) {
			default:
				pbrk[i - 1] = brk; // Save break action in output array
				break;
			case INDIRECT_BREAK:
				// Allowed break if context is A SP + B
				pbrk[i - 1] = pcls[i - 1] == UNICODE_BREAK_SPACE ? INDIRECT_BREAK : PROHIBITED_BREAK;
				break;
			case COMBINING_INDIRECT_BREAK:
				if (pcls[i - 1] == UNICODE_BREAK_SPACE) {
					pbrk[i - 1] = COMBINING_INDIRECT_BREAK; // Apply rule SP ÷
				} else { // Apply rule LB9: X CM * -> X
					pbrk[i - 1] = PROHIBITED_BREAK; // Do not break before CM
					continue; // Do not update cls
				}
				break;
			case COMBINING_PROHIBITED_BREAK: // This is the case OP SP* CM
				pbrk[i - 1] = COMBINING_PROHIBITED_BREAK; // No break allowed
				if (pcls[i - 1] != UNICODE_BREAK_SPACE)
					continue; // Apply rule LB9: X CM* -> X
				break;
		}

		cls = pcls[i]; // Save cls of 'before' character (unless bypassed by 'continue')
	}

	pbrk[i - 1] = EXPLICIT_BREAK; // Always break at the end
	return i;
}

enum GraphemeBreakType getGraphemeBreakType(int codepoint, enum UnicodeType type) {
	// TODO extend this
	if (codepoint == 0x000D) return GB_CR;
	if (codepoint == 0x000A) return GB_LF;
	if (codepoint == 0x200D) return GB_ZWJ;
	switch (type) {
		case UNICODE_NON_SPACING_MARK:
		case UNICODE_ENCLOSING_MARK:
			return GB_EXTEND;
		case UNICODE_FORMAT:
			if (codepoint == 0x200C || codepoint == 0x200D) {
				return GB_EXTEND;
			}
		case UNICODE_CONTROL:
		case UNICODE_LINE_SEPARATOR:
		case UNICODE_PARAGRAPH_SEPARATOR:
		case UNICODE_SURROGATE:
			return GB_CONTROL;
		case UNICODE_SPACING_MARK:
			if (codepoint >= 0x0900) {
				if (codepoint == 0x09BE || codepoint == 0x09D7
						|| codepoint == 0x0B3E || codepoint == 0x0B57 || codepoint == 0x0BBE || codepoint == 0x0BD7
						|| codepoint == 0x0CC2 || codepoint == 0x0CD5 || codepoint == 0x0CD6
						|| codepoint == 0x0D3E || codepoint == 0x0D57 || codepoint == 0x0DCF || codepoint == 0x0DDF
						|| codepoint == 0x1D165 || (codepoint >= 0x1D16E && codepoint <= 0x1D172))
					return GB_EXTEND;
			}
			return GB_SPACING_MARK;
		default:
			return GB_OTHER;
	}
}

int isGraphemeClusterBreak(enum GraphemeBreakType previous, enum GraphemeBreakType current) {
	// GB3. CR × LF
	if (previous == GB_CR && current == GB_LF) return 0;
	// GB4. (Control | CR | LF) ÷
	if (previous == GB_CONTROL || previous == GB_CR || previous == GB_LF) return 1;
	// GB5. ÷ (Control | CR | LF)
	if (current == GB_CONTROL || current == GB_CR || current == GB_LF) return 1;
	// GB6. L × (L | V | LV | LVT)
	if (previous == GB_L && (current == GB_L || current == GB_V || current == GB_LV || current == GB_LVT)) return 0;
	// GB7. (LV | V) × (V | T)
	if ((previous == GB_LV || previous == GB_V) && (current == GB_V || current == GB_T)) return 0;
	// GB8. (LVT | T) × (T)
	if ((previous == GB_LVT || previous == GB_T) && current == GB_T) return 0;
	// GB9. × (Extend | ZWJ)
	if (current == GB_EXTEND || current == GB_ZWJ) return 0;
	// GB9a. × SpacingMark
	if (current == GB_SPACING_MARK) return 0;
	// GB9b. Prepend ×
	if (previous == GB_PREPEND) return 0;
	// GB10. Any ÷ Any
	return 1;
}

/**
 * @param glyphCount
 */
static void calcLogicalSizes(int *logicalSizes, int glyphCount,
		hb_glyph_info_t glyphInfo[static glyphCount], hb_glyph_position_t glyphPos[static glyphCount],
		unsigned int itemOffset, int itemLength) {
	logicalSizes += itemOffset;
	for (int i = 0; i < glyphCount;) {
		int cluster = glyphInfo[i].cluster;
		int clusterSize = 0;
		for (;; ++i) {
			if (i == glyphCount) {
				int numChars = itemLength - cluster;
				for (int j = cluster; j < itemLength; ++j)
					logicalSizes[j - itemOffset] = clusterSize / numChars;
				break;
			}
			if (glyphInfo[i].cluster > cluster) {
				int numChars = glyphInfo[i].cluster - cluster;
				for (int j = cluster; j < glyphInfo[i].cluster; ++j)
					logicalSizes[j - itemOffset] = clusterSize / numChars;
				// Add residue to first char
				logicalSizes[cluster - itemOffset] += clusterSize % numChars;
				break;
			}
			clusterSize += glyphPos[i].x_advance / 64;
		}
	}
}

struct Range {
	/** A HarfBuzz buffer holding the glyphs of this range. */
	hb_buffer_t *buffer;
	/** The inclusive start index of the glyph range. */
	unsigned int start,
				 /** The exclusive end index of the glyph range. */
				 end;

	struct Range *prevLineEnd;
};

/**
 * Returns whether a break is allowed before the codepoint at index i.
 */
static int isBreakAllowed(int i, enum BreakAction *pbrk) {
	if (i == 0) return 0;
	/*if (charBreaks) {
		// TODO cant break before spaces
		const char *text = layout->text; // TODO utf-8
		enum GraphemeBreakType previous = getGraphemeBreakType(text[index - 1], getUnicodeType(text[index - 1])),
							   current = getGraphemeBreakType(text[index], getUnicodeType(text[index]));
		return isGraphemeClusterBreak(previous, current);
	}*/
	enum BreakAction action = pbrk[i - 1];
	return action == DIRECT_BREAK || action == INDIRECT_BREAK || action == COMBINING_INDIRECT_BREAK || action == EXPLICIT_BREAK;
}

/**
 * Returns whether it's safe to break without reshaping before the given glyph.
 */
static int isSafeToBreakBefore(hb_glyph_info_t *glyphInfos, int i) {
	return i == 0 ? 1 // At the start of the run
		: glyphInfos[i - 1].cluster == glyphInfos[i].cluster ? 0 // Not at a cluster boundary
		: (hb_glyph_info_get_glyph_flags(glyphInfos + i) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0;
}

struct BreakSpot {
	int i;
	int safe : 1;
	/** The glyph index to break on, or 0. */
	int glyphIndex;
};

/**
 * Returns the glyph index to split around if applicable.
 */
static struct BreakSpot getBreakSpotFromIndex(struct Range run, int i) {
	unsigned int glyphCount;
	hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(run.buffer, &glyphCount);

	for (int j = run.start; j < run.end; ++j) {
		hb_glyph_info_t info = glyphInfos[j];

		if (info.cluster == i) {
			int safe = isSafeToBreakBefore(glyphInfos, i);
			return (struct BreakSpot) { i, safe, j };
		} else if (info.cluster > i) {
			if (i == run.start) {
				// Break last run: need reshape
				// return (struct BreakSpot) { runIndex - 1, i, 0, 0 };
				assert(0 && "This shouldn't happen.");
			} else {
				// Inside a cluster: need reshape
				return (struct BreakSpot) { i, 0, 0 };
			}
		}
	}

	// Else break last run
	return (struct BreakSpot) { i, 0, 0 };
}

static struct Range shapeRange(hb_font_t *font, const char *text, int textLength, unsigned int itemOffset, int itemLength) {
	hb_buffer_t *buffer = hb_buffer_create();
	hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
	hb_buffer_add_utf8(buffer, text, textLength, itemOffset, itemLength);
	hb_buffer_guess_segment_properties(buffer);
	hb_shape(font, buffer, NULL, 0);

	unsigned int glyphCount;
	(void) hb_buffer_get_glyph_infos(buffer, &glyphCount);
	return (struct Range) { buffer, 0, glyphCount };
}

static void calcLogicalSizesOfRun(int *logicalSizes, struct Range run, unsigned int itemOffset, int itemLength) {
	unsigned int glyphCount;
	hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(run.buffer, &glyphCount);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(run.buffer, NULL);
	calcLogicalSizes(logicalSizes, run.end - run.start, infos + run.start, pos + run.start, itemOffset, itemLength);
}

static int glyphStringGetWidth(struct GlyphString *glyphs) {
	int width = 0;
	for (int i = 0; i < glyphs->length; ++i) {
		width += glyphs->infos[i].width;
	}
	return width;
}

static void addLineFromRun(struct Layout *layout, struct Range run) {
	if (run.prevLineEnd) addLineFromRun(layout, *run.prevLineEnd);

	hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(run.buffer, NULL);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(run.buffer, NULL);

	struct LayoutLine *line = layout->lines + layout->lineCount++;
	line->itemCount = 0;
	struct LayoutItem *item = malloc(sizeof(struct LayoutItem));
	line->items[line->itemCount++] = item;

	unsigned int glyphCount = run.end - run.start;
	struct GlyphString *glyphs = item->glyphs = malloc(sizeof(struct GlyphString) + sizeof(struct GlyphInfo) * glyphCount);
	glyphs->length = glyphCount;
	for (int i = 0, j = run.start; j < run.end; ++i, ++j) {
		glyphs->infos[i] = (struct GlyphInfo) {
			infos[j].codepoint, pos[j].x_advance / 64,
				pos[j].x_offset / 64, pos[j].y_offset / 64
		};
	}
}

void layoutLayout(struct Layout *layout) {
	hb_font_t *font = layout->font->hbFont;
	const char *text = layout->text;
	int textLength = layout->length;
	int remainingWidth = layout->width;
	assert(remainingWidth != -1);

	// Assign a line breaking class to each code point
	enum BreakClass *pcls = malloc(sizeof(enum BreakClass) * textLength);
	if (!pcls) assert(0);
	for (int i = 0; i < textLength; ++i) pcls[i] = getBreakClass(text[i]);
	enum BreakAction *pbrk = malloc(sizeof(enum BreakAction) * textLength);
	if (!pbrk) assert(0);
	// Per codepoint advances
	int *logicalSizes;
	if (!(logicalSizes = malloc(textLength * sizeof *logicalSizes))) {
		fprintf(stderr, "Error\n");
		return;
	}

	struct Range runs[64]; // TODO dynamically grow
	struct Range *runPointer = runs; // Pointer to next run

	// Itemize - first run is special
	*runPointer++ = shapeRange(font, text, textLength, 0, textLength);
	calcLogicalSizesOfRun(logicalSizes, runs[0], 0, textLength);

	int i = 0;
	while (i < textLength) {
		int lineStart = i; // Codepoint index of the start of current line
		int explicitBreak = findLineBreak(pcls + i, pbrk + i, textLength - i) + i;
		// printf("top of main loop, i=%d, explicitBreak=%d\n", i, explicitBreak);
		for (int width = 0; i < explicitBreak; ++i) {
			width += logicalSizes[i];
			if (width > remainingWidth) break;
		}
		if (i >= textLength) goto finishLayout;

		struct BreakSpot breakSpot;
		// Search backwards for break opportunity
		while (i > lineStart) {
			if (isBreakAllowed(i, pbrk)) {
				// Find glyph cluster from codepoint index i
				breakSpot = getBreakSpotFromIndex(runs[0], i);
				goto foundBreak;
			}
			--i;
		}
		assert(0 && "Emergency mode not yet implemented.");

foundBreak:
		// Break before i
		if (breakSpot.safe) {
			struct Range *end = runPointer++;
			*end = (struct Range) { hb_buffer_reference(runs[0].buffer), runs[0].start, breakSpot.glyphIndex, runs[0].prevLineEnd };
			runs[0] = (struct Range) { hb_buffer_reference(runs[0].buffer), breakSpot.glyphIndex, runs[0].end, end };
		} else {
			// Reshaping is necessary
			struct Range run = runs[0], *nextRun = 0;
			unsigned int itemOffset = hb_buffer_get_glyph_infos(run.buffer, NULL)[run.start].cluster;
			hb_buffer_destroy(runs[0].buffer);
			struct Range *last = runPointer++;
			*last = shapeRange(font, text, textLength, itemOffset, i - itemOffset);
			last->prevLineEnd = runs[0].prevLineEnd;

			int itemLength = (nextRun ? hb_buffer_get_glyph_infos(nextRun->buffer, NULL)[nextRun->start].cluster : textLength) - i;
			runs[0] = shapeRange(font, text, textLength, i, itemLength);
			runs[0].prevLineEnd = last;
			calcLogicalSizesOfRun(logicalSizes, runs[0], i, itemLength);
		}
	}

finishLayout:
	free(logicalSizes);
	free(pbrk);
	free(pcls);

	layout->lineCount = 0;
	addLineFromRun(layout, runs[0]);
}

void layoutInit(struct Layout *layout, struct Font *font) {
	layout->font = font;
	layout->width = -1;
	layout->height = -1;
	layout->text = "";
	layout->length = 0;
	layout->lineCount = -1;
}

void layoutSetText(struct Layout *layout, const char *text, int length) {
	layout->text = text;
	layout->length = length < 0 ? strlen(text) : length;
	layoutDestroy(layout);
}

void layoutSetWidth(struct Layout *layout, int width) {
	if (width < 0) width = -1;
	if (width != layout->width) {
		layout->width = width;
		layoutDestroy(layout);
	}
}

void layoutSetHeight(struct Layout *layout, int height) {
	if (height < 0) height = -1;
	if (height != layout->height) {
		layout->height = height;
		layoutDestroy(layout);
	}
}

void layoutGetSize(struct Layout *layout, int *width, int *height) {
	if (layout->lineCount == -1) layoutLayout(layout);
	int layoutWidth = 0;
	int layoutHeight = 0;
	for (int i = 0; i < layout->lineCount; ++i) {
		struct LayoutLine *line = layout->lines + i;
		for (int j = 0; j < line->itemCount; ++j) {
			struct LayoutItem *item = line->items[j];
			layoutWidth += glyphStringGetWidth(item->glyphs);
		}
		layoutHeight += layout->font->lineSpacing;
	}
	*width = layoutWidth;
	*height = layoutHeight;
}

void layoutDestroy(struct Layout *layout) {
	// TODO choose other than -1 implement real arrays
	if (layout->lineCount != -1) {
		for (int i = 0; i < layout->lineCount; ++i) {
			struct LayoutLine *line = layout->lines + i;
			for (int j = 0; j < line->itemCount; ++j) {
				struct LayoutItem *item = line->items[j];
				free(item->glyphs);
				item->glyphs = 0;
			}
		}
		layout->lineCount = -1;
	}
}
