local mappings = {
	-- Normative
	Lu = "UNICODE_UPPERCASE_LETTER",
	Ll = "UNICODE_LOWERCASE_LETTER",
	Lt = "UNICODE_TITLECASE_LETTER",
	Mn = "UNICODE_NON_SPACING_MARK",
	Mc = "UNICODE_SPACING_MARK",
	Me = "UNICODE_ENCLOSING_MARK",
	Nd = "UNICODE_DECIMAL_NUMBER",
	Nl = "UNICODE_LETTER_NUMBER",
	No = "UNICODE_OTHER_NUMBER",
	Zs = "UNICODE_SPACE_SEPARATOR",
	Zl = "UNICODE_LINE_SEPARATOR",
	Zp = "UNICODE_PARAGRAPH_SEPARATOR",
	Cc = "UNICODE_CONTROL",
	Cf = "UNICODE_FORMAT",
	Cs = "UNICODE_SURROGATE",
	Co = "UNICODE_PRIVATE_USE",
	Cn = "UNICODE_UNASSIGNED",
	-- Informative.
	Lm = "UNICODE_MODIFIER_LETTER",
	Lo = "UNICODE_OTHER_LETTER",
	Pc = "UNICODE_CONNECT_PUNCTUATION",
	Pd = "UNICODE_DASH_PUNCTUATION",
	Ps = "UNICODE_OPEN_PUNCTUATION",
	Pe = "UNICODE_CLOSE_PUNCTUATION",
	Pi = "UNICODE_INITIAL_PUNCTUATION",
	Pf = "UNICODE_FINAL_PUNCTUATION",
	Po = "UNICODE_OTHER_PUNCTUATION",
	Sm = "UNICODE_MATH_SYMBOL",
	Sc = "UNICODE_CURRENCY_SYMBOL",
	Sk = "UNICODE_MODIFIER_SYMBOL",
	So = "UNICODE_OTHER_SYMBOL"
}

local breakMappings = {
	AI = "UNICODE_BREAK_AMBIGUOUS",
	AL = "UNICODE_BREAK_ALPHABETIC",
	B2 = "UNICODE_BREAK_BEFORE_AND_AFTER",
	BA = "UNICODE_BREAK_AFTER",
	BB = "UNICODE_BREAK_BEFORE",
	BK = "UNICODE_BREAK_MANDATORY",
	CB = "UNICODE_BREAK_CONTINGENT",
	CJ = "UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER",
	CL = "UNICODE_BREAK_CLOSE_PUNCTUATION",
	CM = "UNICODE_BREAK_COMBINING_MARK",
	CP = "UNICODE_BREAK_CLOSE_PARENTHESIS",
	CR = "UNICODE_BREAK_CARRIAGE_RETURN",
	EB = "UNICODE_BREAK_EMOJI_BASE",
	EM = "UNICODE_BREAK_EMOJI_MODIFIER",
	EX = "UNICODE_BREAK_EXCLAMATION",
	GL = "UNICODE_BREAK_NON_BREAKING_GLUE",
	H2 = "UNICODE_BREAK_HANGUL_LV_SYLLABLE",
	H3 = "UNICODE_BREAK_HANGUL_LVT_SYLLABLE",
	HL = "UNICODE_BREAK_HEBREW_LETTER",
	HY = "UNICODE_BREAK_HYPHEN",
	ID = "UNICODE_BREAK_IDEOGRAPHIC",
	IN = "UNICODE_BREAK_INSEPARABLE",
	IS = "UNICODE_BREAK_INFIX_SEPARATOR",
	JL = "UNICODE_BREAK_HANGUL_L_JAMO",
	JT = "UNICODE_BREAK_HANGUL_T_JAMO",
	JV = "UNICODE_BREAK_HANGUL_V_JAMO",
	LF = "UNICODE_BREAK_LINE_FEED",
	NL = "UNICODE_BREAK_NEXT_LINE",
	NS = "UNICODE_BREAK_NON_STARTER",
	NU = "UNICODE_BREAK_NUMERIC",
	OP = "UNICODE_BREAK_OPEN_PUNCTUATION",
	PO = "UNICODE_BREAK_POSTFIX",
	PR = "UNICODE_BREAK_PREFIX",
	QU = "UNICODE_BREAK_QUOTATION",
	RI = "UNICODE_BREAK_REGIONAL_INDICATOR",
	SA = "UNICODE_BREAK_COMPLEX_CONTEXT",
	SG = "UNICODE_BREAK_SURROGATE",
	SP = "UNICODE_BREAK_SPACE",
	SY = "UNICODE_BREAK_SYMBOL",
	WJ = "UNICODE_BREAK_WORD_JOINER",
	XX = "UNICODE_BREAK_UNKNOWN",
	ZW = "UNICODE_BREAK_ZERO_WIDTH_SPACE",
	ZWJ = "UNICODE_BREAK_ZERO_WIDTH_JOINER"
}
local hashtag = string.byte("#")

print("Reading UnicodeData.txt.")

print("Creating line break table")

local f = io.open("unicodeTables.h", "w")
f:write([[/* This file was automatically generated. DO NOT EDIT! */

#ifndef UNICODE_TABLES_H
#define UNICODE_TABLES_H
]])

function getDefaultValue(codepoint)
	if codepoint >= 0x3400 and codepoint <= 0x4DBF
		or codepoint >= 0x4E00 and codepoint <= 0x9FFF
		or codepoint >= 0xF900 and codepoint <= 0xFAFF then
		return "ID"
	elseif codepoint >= 0x20000 and codepoint <= 0x2FFFD
		or codepoint >= 0x30000 and codepoint <= 0x3FFFD then
		return "ID"
	elseif codepoint >= 0x1F000 and codepoint <= 0x1FFFD then
		return "ID"
	elseif codepoint >= 0x20A0 and codepoint <= 0x20CF then
		return "PR"
	else
		return "XX"
	end
end

function createTableBuilder (f, label)
	local builder = {}
	local SIZE = 256
	local i = 0
	local page = {}
	local pageCount = 0
	local equalFlag = true -- Whether all values are equal
	-- Dictionary of page id to data page or line break property
	local rows = {}
	upperLabel = string.upper(label)

	builder.printPage = function()
		if equalFlag then
			table.insert(rows, page[0] .. " + UNICODE_MAX_" .. upperLabel .. "_TABLE_INDEX")
		else
			table.insert(rows, pageCount)

			f:write("\t{ /* page ", pageCount, " */\n")
			for code = 0, 255, 2 do
				local first, second = page[code], page[code + 1]
				if not (first and second) then error("Less than 256 codepoints.") end
				f:write("\t\t", first, ", ", second, ",\n")
			end
			f:write("\t},\n")

			pageCount = pageCount + 1
		end
	end

	builder.addEntry = function (value)
		page[i] = value
		if equalFlag and i ~= 0 and page[i] ~= page[0] then
			equalFlag = false
		end

		i = i + 1

		if i >= SIZE then
			builder.printPage()
			i = 0
			equalFlag = true
		end
	end

	builder.printDataStart = function (type)
		f:write("static const " .. type .. " " .. label .. "PropertyData[][256] = {\n")
	end

	builder.printTable = function (type)
		f:write("};\n\n#define UNICODE_MAX_", upperLabel, "_TABLE_INDEX ", pageCount, "\n\nstatic const ", type, " ", label, "PropertyTable[] = {\n")
		for i, v in ipairs(rows) do
			f:write("\t", v, ",\n")
		end
		f:write("};")
	end

	return builder
end

local breakBuilder = createTableBuilder(f, "break")
breakBuilder.printDataStart("BreakClass")
local last = -1
for line in io.lines("LineBreak.txt") do
	-- Hashtag starts a comment
	if line:byte() ~= hashtag and line ~= "" then
		-- Parse codepoint, optional range and line break property
		local start, stop, lineBreakProperty = line:match("^([%dA-F]*)%.*([%dA-F]*);(%w+)")
		start = tonumber(start, 16)
		stop = stop == "" and start or tonumber(stop, 16) -- Range is optional

		for code = last + 1, stop do
			value = breakMappings[code < start and getDefaultValue(code) or lineBreakProperty]
			breakBuilder.addEntry(value)
		end
		last = stop
	end
end
breakBuilder.printPage()
breakBuilder.printTable("unsigned char")

f:write("\n\n");

local typeBuilder = createTableBuilder(f, "type")
typeBuilder.printDataStart("enum UnicodeType")
last = -1
for line in io.lines("UnicodeData.txt") do
	local code, name, type = line:match("^([%dA-F]*);([^;]*);(%a+)")
	code = tonumber(code, 16)
	type = mappings[type]

	if code > last + 1 then
		if not string.find(name, "Last>") then
			type = mappings["Cn"]
		end
		while last < code do
			last = last + 1
			typeBuilder.addEntry(type);
		end
	end
	typeBuilder.addEntry(type)
	last = code
end
typeBuilder.printPage()
typeBuilder.printTable("unsigned char");

f:write("\n\n#endif\n")

f:close();
