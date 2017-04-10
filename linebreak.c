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

// placeholder function for complex break analysis
// cls - resolved line break class, may differ from pcls[0]
// pcls - pointer to array of line breaking classes (input)
// pbrk - pointer to array of line breaking opportunities (output)
// cch - remaining length of input
/*int findComplexBreak(enum break_class cls, enum break_class *pcls, enum break_action *pbrk, int cch) {
  if (!cch) return 0;
  for (int i = 1; i < cch; ++i) {
// .. do complex break analysis here
// and report any break opportunities in pbrk ..
pbrk[i-1] = PROHIBITED_BRK; // by default, no break

if (pcls[i] != SA) break;
}
return i;
}*/

// handle spaces separately, all others by table
// pcls - pointer to array of line breaking classes (input)
// pbrk - pointer to array of line break opportunities (output)
// length - number of elements in the arrays (“count of characters”) (input)
// current index into the arrays (variable) (returned value)
int findLineBreak(enum BreakClass *pcls, enum BreakAction *pbrk, int length) {
	if (!length) return 0;

	enum BreakClass cls = pcls[0]; // Class of 'before' character
	// Treat SP at start of input as if it followed a WJ
	if (cls == UNICODE_BREAK_SPACE) cls = UNICODE_BREAK_WORD_JOINER;
	// Handle case where input starts with an LF and treat initial NL like BK
	if (cls == UNICODE_BREAK_LINE_FEED || cls == UNICODE_BREAK_NEXT_LINE) cls = UNICODE_BREAK_MANDATORY;

	// loop over all pairs in the string up to a hard break or CRLF pair
	int i = 1;
	for (; i < length && cls != UNICODE_BREAK_MANDATORY
			&& (cls != UNICODE_BREAK_CARRIAGE_RETURN || pcls[i] == UNICODE_BREAK_LINE_FEED); ++i) {
		// to handle explicit breaks, replace code from "for" loop condition
		// above to comment below by code given in Section 7.6

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
			pbrk[i-1] = PROHIBITED_BREAK; // apply rule LB7: × SP
			continue; // do not update cls
		}

		// Handle complex scripts in a separate function
		if (pcls[i] == UNICODE_BREAK_COMPLEX_CONTEXT) {
			return -1;
			// i += findComplexBreak(cls, &pcls[i - 1], &pbrk[i - 1], cch - (i - 1));
			if (i < length) cls = pcls[i];
			continue;
		}

		// Lookup pair table information in breakPairs[before, after];
		int first = lineBreakIndices[cls], second = lineBreakIndices[pcls[i]];
		assert(first < INDEX_END_OF_TABLE && second < INDEX_END_OF_TABLE && "Index out of bounds.");
		enum BreakAction brk = breakPairs[lineBreakIndices[cls]][lineBreakIndices[pcls[i]]];
		pbrk[i - 1] = brk; // save break action in output array

		// Resolve indirect break
		// Handle breaks involving a combining mark (see Section 7.5)

		if (brk == INDIRECT_BREAK) {
			// If context is A SP + B
			if (pcls[i - 1] == UNICODE_BREAK_SPACE)
				pbrk[i - 1] = INDIRECT_BREAK; // Break opportunity
			else
				pbrk[i - 1] = PROHIBITED_BREAK; // No break opportunity
		} else if (brk == COMBINING_INDIRECT_BREAK) { // Resolve combining mark break
			pbrk[i - 1] = PROHIBITED_BREAK;             // do not break before CM
			if (pcls[i - 1] == UNICODE_BREAK_SPACE) {
				pbrk[i - 1] = COMBINING_INDIRECT_BREAK; // Apply rule SP ÷
			} else                                   // apply rule LB9: X CM * -> X
				continue;                            // do not update cls
		} else if (brk == COMBINING_PROHIBITED_BREAK) { // This is the case OP SP* CM
			pbrk[i - 1] = COMBINING_PROHIBITED_BREAK; // No break allowed
			if (pcls[i - 1] != UNICODE_BREAK_SPACE)
				continue;                          // apply rule LB9: X CM* -> X
		}

		// Save cls of 'before' character (unless bypassed by 'continue')
		cls = pcls[i];
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

	}
	return GB_OTHER;
}


int isGraphemeClusterBreak(enum GraphemeBreakType previous, enum GraphemeBreakType current) {
	// GB3. CR X LF
	if (previous == GB_CR && current == GB_LF) return 0;
	// GB4. (Control | CR | LF) ÷
	if (previous == GB_CONTROL || previous == GB_CR || previous == GB_LF) return 1;
	// GB5. ÷ (Control | CR | LF)
	if (current == GB_CONTROL || current == GB_CR || current == GB_LF) return 1;
	// GB6. L X (L | V | LV | LVT)
	// TODO
	// GB7. (LV | V) X (V | T)
	// TODO
	// GB8. (LVT | T) X (T)
	// TODO
	// GB9. X (Extend | ZWJ)
	if (current == GB_EXTEND || current == GB_ZWJ) return 0;
	// GB9a. X SpacingMark
	if (current == GB_SPACING_MARK) return 0;
	// GB9b. Prepend X
	if (previous == GB_PREPEND) return 0;
	// TODO implement rest
	// GB10. Any ÷ Any
	return 1;
}

static int glyphStringGetWidth(struct GlyphString *glyphs) {
	int width = 0;
	for (int i = 0; i < glyphs->length; ++i) {
		width += glyphs->infos[i].width;
	}
	return width;
}

static struct LayoutItem *splitItem(struct LayoutItem *item, int index) {
	struct LayoutItem *newItem = malloc(sizeof(struct LayoutItem));
	if (!newItem) {
		return 0;
	}
	newItem->offset = item->offset;
	newItem->length = index;
	newItem->glyphs = 0;

	item->offset += index;
	item->length -= index;

	return newItem;
}

static void shapeItem(struct Layout *layout, struct LayoutItem *item, int *logicalWidths) {
	struct Font *font = layout->font;
	const char *paragraphText = layout->text;
	int paragraphLength = layout->length, itemLength = item->length;
	unsigned int itemOffset = item->offset;

	// TODO recycle buffer
	hb_buffer_t *buffer = hb_buffer_create();
	// hb_buffer_reset(buffer);
	hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
	// hb_buffer_set_language(buffer, hb_language_from_string("en", strlen("en")));
	hb_buffer_add_utf8(buffer, paragraphText, paragraphLength, itemOffset, itemLength);
	hb_buffer_guess_segment_properties(buffer);
	hb_shape(font->hbFont, buffer, NULL, 0);
	unsigned int glyphCount;
	hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
	hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(buffer, 0);

	struct GlyphString *glyphs = malloc(sizeof(struct GlyphString) + sizeof(struct GlyphInfo) * glyphCount);
	glyphs->length = glyphCount;
	for (int i = 0; i < glyphCount; ++i) {
		struct GlyphInfo info;
		/*char glyphName[30];
		  if (FT_Get_Glyph_Name(font->face, glyphInfo[i].codepoint, glyphName, sizeof glyphName) == 0) {
		  printf("codepoint: %d (%s), width: %d, xoffset: %d, yoffset: %d, cluster: %d\n", glyphInfo[i].codepoint, glyphName, glyphPos[i].x_advance / 64, glyphPos[i].x_offset / 64, glyphPos[i].y_offset / 64, glyphInfo[i].cluster);
		  }*/
		info.glyph = glyphInfo[i].codepoint;
		info.width = glyphPos[i].x_advance / 64;
		info.xOffset = glyphPos[i].x_offset / 64;
		info.yOffset = glyphPos[i].y_offset / 64;
		glyphs->infos[i] = info;

	}
	item->glyphs = glyphs;

	if (logicalWidths) {
		for (int i = 0; i < glyphCount;) {
			int cluster = glyphInfo[i].cluster;
			int clusterWidth = 0;
			for (;; ++i) {
				if (i == glyphCount) {
					int numChars = itemLength - cluster;
					for (int j = cluster; j < itemLength; ++j) {
						logicalWidths[j - item->offset] = clusterWidth / numChars;
					}
					break;
				}
				if (glyphInfo[i].cluster > cluster) {
					int numChars = glyphInfo[i].cluster - cluster;
					for (int j = cluster; j < glyphInfo[i].cluster; ++j) {
						logicalWidths[j - item->offset] = clusterWidth / numChars;
					}
					logicalWidths[cluster - item->offset] += clusterWidth % numChars; // Add residue to first char
					break;
				}
				clusterWidth += glyphPos[i].x_advance / 64;
			}
		}
	}

	hb_buffer_destroy(buffer);
}

static int canBreakAt(struct Layout *layout, int index, enum BreakAction *pbrk, int charBreaks) {
	if (index == 0) return 0;
	// TODO cant break before spaces
	if (charBreaks) {
		const char *text = layout->text; // TODO utf-8
		enum GraphemeBreakType previous = getGraphemeBreakType(text[index - 1], getUnicodeType(text[index - 1])),
							   current = getGraphemeBreakType(text[index], getUnicodeType(text[index]));
		return isGraphemeClusterBreak(previous, current);
	}
	enum BreakAction action = pbrk[index - 1];
	return action == DIRECT_BREAK || action == INDIRECT_BREAK || action == COMBINING_INDIRECT_BREAK || action == EXPLICIT_BREAK;
}

static struct LayoutLine *createLine(struct Layout *layout) {
	struct LayoutLine *line = layout->lines + layout->lineCount++;
	line->itemCount = 0;
	return line;
}

void layoutLayout(struct Layout *layout) {
	if (layout->lineCount != -1) return;

	int length = layout->length;
	const char *text = layout->text;
	enum BreakClass *pcls = malloc(sizeof(enum BreakClass) * layout->length);
	for (int i = 0; i < layout->length; ++i) {
		pcls[i] = getBreakClass(text[i]);
	}
	enum BreakAction *pbrk = malloc(sizeof(enum BreakAction) * layout->length);
	int ich = findLineBreak(pcls, pbrk, length);
	/*const char *breakActionToString[6] = {
		"DIRECT_BREAK",
		"INDIRECT_BREAK",
		"COMBINING_INDIRECT_BREAK",
		"COMBINING_PROHIBITED_BREAK",
		"PROHIBITED_BREAK",
		"EXPLICIT_BREAK"
	};
	for (int i = 0; i < len; ++i) {
		printf("ch: %c break action: %d - %s.\n", text[i], pbrk[i], breakActionToString[pbrk[i]]);
	}*/

	// TODO dynamically grow stack
	struct LayoutItem **itemStack = malloc(sizeof(struct LayoutItem *) * 10);
	int itemStackTop = 0;

	// TODO itemize
	struct LayoutItem *item0 = malloc(sizeof(struct LayoutItem));
	item0->offset = 0;
	item0->length = length;
	item0->glyphs = 0;

	itemStack[itemStackTop++] = item0;

	layout->lineCount = 0;
	struct LayoutLine *line = createLine(layout);
	int remainingWidth = layout->width;

	int haveBreak = 0,
		breakRemainingWidth = 0,
		breakItemIndex = 0;

	while (itemStackTop > 0) {
		struct LayoutItem *item = itemStack[--itemStackTop];
		int *logicalWidths = malloc(sizeof(int) * item->length); // TODO have one single for the whole layout
		if (item->glyphs) {
			assert(0 && "This shouldn't happen.");
		}
		shapeItem(layout, item, logicalWidths);
		// Width of item
		int width = glyphStringGetWidth(item->glyphs);
		// printf("%d needs to fit into %d.\n", width, remainingWidth);
		int forceFit = !haveBreak;

		// Infinite width
		if (remainingWidth < 0) {
			line->items[line->itemCount++] = item;
			continue;
		}

		int retryingWithCharBreaks = 0;
		int breakWidth = 0, breakNumChars = -1;
retryBreak:
		breakWidth = width;
		breakNumChars = -1;
		for (int i = 0, width = 0; i < item->length; ++i) {
			if (width > remainingWidth && breakNumChars != -1)
				break;
			if (canBreakAt(layout, item->offset + i, pbrk, retryingWithCharBreaks)
					&& (i > 0 || line->itemCount)) {
				breakNumChars = i;
				breakWidth = width;
			}
			width += logicalWidths[i];
		}

		if (forceFit && breakWidth > remainingWidth && !retryingWithCharBreaks) {
			retryingWithCharBreaks = 1;
			goto retryBreak;
		}

		if (width <= remainingWidth) {
			// All fit
			// printf("All fit.\n");
			if (breakNumChars != -1) {
				haveBreak = 1;
				breakRemainingWidth = remainingWidth;
				breakItemIndex = line->itemCount - 1;
			}
			remainingWidth -= width;
			line->items[line->itemCount++] = item;
		} else if (breakWidth <= remainingWidth || !haveBreak) {
			// TODO remove width of space character at end of item before checking
			if (breakNumChars == item->length) {
				remainingWidth -= width;
				line->items[line->itemCount++] = item;
			} else {
				if (breakNumChars > 0) {
					// Some fit
					// printf("Some fit.\n");
					struct LayoutItem *newItem = splitItem(item, breakNumChars);
					shapeItem(layout, newItem, 0);
					line->items[line->itemCount++] = newItem;
					remainingWidth -= glyphStringGetWidth(newItem->glyphs);
				} else {
					// printf("Empty fit.\n");
				}

				free(item->glyphs);
				item->glyphs = 0;
				itemStack[itemStackTop++] = item;
			}

			line = createLine(layout);
			remainingWidth = layout->width;
		} else {
			assert(0 && "Not yet implemented.");
			assert(haveBreak && "Should have a break spot.");
			// None fit
			// printf("None fit.\n");
			remainingWidth = breakRemainingWidth;

			free(item->glyphs);
			item->glyphs = 0;
			itemStack[itemStackTop++] = item; // Add item back on stack

			/*struct LayoutItem *newItem = splitItem(item, breakNumChars);
			  free(item->glyphs);
			  item->glyphs = 0;*/

			// Back up over unused items to item where there is a break
			// for (int i = breakItemIndex + 1; i < line->itemCount; ++i) {
			for (int i = itemStackTop; i-- > breakItemIndex;) {
				itemStack[itemStackTop++] = line->items[i];
			}
			line->itemCount = breakItemIndex;

			shapeItem(layout, item, 0);
			line->items[line->itemCount++] = item;
			remainingWidth -= glyphStringGetWidth(item->glyphs);
		}

	free(logicalWidths);
	}

	free(itemStack);
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
	layoutLayout(layout);
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
