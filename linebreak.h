#ifndef LINEBREAK_H
#define LINEBREAK_H

#include <hb.h>
#include "font.h"

typedef enum UnicodeType {
	// Normative
	UNICODE_UPPERCASE_LETTER,
	UNICODE_LOWERCASE_LETTER,
	UNICODE_TITLECASE_LETTER,
	UNICODE_NON_SPACING_MARK,
	UNICODE_SPACING_MARK,
	UNICODE_ENCLOSING_MARK,
	UNICODE_DECIMAL_NUMBER,
	UNICODE_LETTER_NUMBER,
	UNICODE_OTHER_NUMBER,
	UNICODE_SPACE_SEPARATOR,
	UNICODE_LINE_SEPARATOR,
	UNICODE_PARAGRAPH_SEPARATOR,
	UNICODE_CONTROL,
	UNICODE_FORMAT,
	UNICODE_SURROGATE,
	UNICODE_PRIVATE_USE,
	UNICODE_UNASSIGNED,
	// Informative
	UNICODE_MODIFIER_LETTER,
	UNICODE_OTHER_LETTER,
	UNICODE_CONNECT_PUNCTUATION,
	UNICODE_DASH_PUNCTUATION,
	UNICODE_OPEN_PUNCTUATION,
	UNICODE_CLOSE_PUNCTUATION,
	UNICODE_INITIAL_PUNCTUATION,
	UNICODE_FINAL_PUNCTUATION,
	UNICODE_OTHER_PUNCTUATION,
	UNICODE_MATH_SYMBOL,
	UNICODE_CURRENCY_SYMBOL,
	UNICODE_MODIFIER_SYMBOL,
	UNICODE_OTHER_SYMBOL,
} UnicodeType;

enum UnicodeType getUnicodeType(int codepoint);

typedef enum BreakClass {
	UNICODE_BREAK_MANDATORY,
	UNICODE_BREAK_CARRIAGE_RETURN,
	UNICODE_BREAK_LINE_FEED,
	UNICODE_BREAK_COMBINING_MARK,
	UNICODE_BREAK_SURROGATE,
	UNICODE_BREAK_ZERO_WIDTH_SPACE,
	UNICODE_BREAK_INSEPARABLE,
	UNICODE_BREAK_NON_BREAKING_GLUE,
	UNICODE_BREAK_CONTINGENT,
	UNICODE_BREAK_SPACE,
	UNICODE_BREAK_AFTER,
	UNICODE_BREAK_BEFORE,
	UNICODE_BREAK_BEFORE_AND_AFTER,
	UNICODE_BREAK_HYPHEN,
	UNICODE_BREAK_NON_STARTER,
	UNICODE_BREAK_OPEN_PUNCTUATION,
	UNICODE_BREAK_CLOSE_PUNCTUATION,
	UNICODE_BREAK_QUOTATION,
	UNICODE_BREAK_EXCLAMATION,
	UNICODE_BREAK_IDEOGRAPHIC,
	UNICODE_BREAK_NUMERIC,
	UNICODE_BREAK_INFIX_SEPARATOR,
	UNICODE_BREAK_SYMBOL,
	UNICODE_BREAK_ALPHABETIC,
	UNICODE_BREAK_PREFIX,
	UNICODE_BREAK_POSTFIX,
	UNICODE_BREAK_COMPLEX_CONTEXT,
	UNICODE_BREAK_AMBIGUOUS,
	UNICODE_BREAK_UNKNOWN,
	UNICODE_BREAK_NEXT_LINE,
	UNICODE_BREAK_WORD_JOINER,
	UNICODE_BREAK_HANGUL_L_JAMO,
	UNICODE_BREAK_HANGUL_V_JAMO,
	UNICODE_BREAK_HANGUL_T_JAMO,
	UNICODE_BREAK_HANGUL_LV_SYLLABLE,
	UNICODE_BREAK_HANGUL_LVT_SYLLABLE,
	UNICODE_BREAK_CLOSE_PARENTHESIS,
	UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER,
	UNICODE_BREAK_HEBREW_LETTER,
	UNICODE_BREAK_REGIONAL_INDICATOR,
	UNICODE_BREAK_EMOJI_BASE,
	UNICODE_BREAK_EMOJI_MODIFIER,
	UNICODE_BREAK_ZERO_WIDTH_JOINER
} BreakClass;

/**
 * Returns the break class of the specified codepoint.
 *
 * @param codepoint The codepoint.
 * @return The Unicode break class.
 */
enum BreakClass getBreakClass(int codepoint);

typedef enum BreakAction {
	DIRECT_BREAK,
	INDIRECT_BREAK,
	COMBINING_INDIRECT_BREAK,
	COMBINING_PROHIBITED_BREAK,
	PROHIBITED_BREAK,
	EXPLICIT_BREAK
} BreakAction;

/**
 * Finds line break opportunities up to a explicit line break.
 *
 * @param pcls Pointer to array of input line breaking classes.
 * @param pbrk Pointer to array of output line break opportunities.
 * @param length Number of elements in the arrays.
 * @return The current index into the arrays.
 */
int findLineBreak(enum BreakClass *pcls, enum BreakAction *pbrk, int length);

enum GraphemeBreakType {
	GB_CR,
	GB_LF,
	GB_CONTROL,
	GB_EXTEND,
	GB_ZWJ,
	GB_REGIONAL_INDICATOR,
	GB_PREPEND,
	GB_SPACING_MARK,
	GB_L,
	GB_V,
	GB_T,
	GB_LV,
	GB_LVT,
	GB_E_BASE,
	GB_E_MODIFIER,
	GB_GLUE_AFTER_ZWJ,
	GB_BASE_GAZ,
	GB_OTHER
};

enum GraphemeBreakType getGraphemeBreakType(int codepoint, enum UnicodeType);

int isGraphemeClusterBreak(enum GraphemeBreakType previous, enum GraphemeBreakType current);

struct GlyphInfo {
	int glyph;
	int width;
	int xOffset;
	int yOffset;
};

struct GlyphString {
	int length;
	struct GlyphInfo infos[];
};

struct LayoutItem {
	unsigned int offset;
	int length;
	struct GlyphString *glyphs;
};

struct LayoutLine {
	struct LayoutItem *items[16];
	int itemCount;
};

struct Layout {
	struct Font *font;
	int width;
	int height;
	const char *text;
	int length;
	struct LayoutLine lines[16];
	int lineCount;
};

// TODO take string length and string as arguments
void layoutLayout(struct Layout *layout);

void layoutInit(struct Layout *layout, struct Font *font);

void layoutSetText(struct Layout *layout, const char *text, int length);

void layoutSetWidth(struct Layout *layout, int width);

void layoutSetHeight(struct Layout *layout, int height);

void layoutGetSize(struct Layout *layout, int *width, int *height);

void layoutDestroy(struct Layout *layout);

#endif
