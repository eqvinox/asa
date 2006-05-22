/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005  David Lamparter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#include "common.h"
#include "asaerror.h"

struct asaec_desc ssaec[ASAEC_MAX] = {
/* 0 */
{0, "no error",
	"",
	NULL},
/* 1 SSAEC_NOTSUP */
{3, "command/option not supported",
	"ASA does not have parse support for this option/style yet.\n\n"
	"This currently includes:\n- embedded fonts\n- embedded graphics"
	"\n- drawing commands (\\p and \\clip with paint commands)",
	NULL},
/* 2 SSAEC_UNKNOWNVER */
{3, "unknown script version",
	"The ASA parser was not able to recognize the given script "
	"version.\n\nCurrently supported are 'v4.00' and 'v4.00+'",
	NULL},
/* 3 SSAEC_AMBIGUOUS_SVER */
{3, "ambiguous script version",
	"Two or more script version indicators are present in this script, "
	"not all of them indicating the same version.\n\nCheck for and "
	"remove incorrect script version indicators. (this includes "
	"'[v4/v4+ Styles]')\nASA is assuming ASS 4.00+ for this run.",
	NULL},
/* 4 SSAEC_INVAL_ENCODING */
{3, "invalid input encoding",
	"An nonconvertible, probably invalid byte sequence caused "
	"character set conversion to fail.\n\nMost of the cases, this is "
	"either a result of an incorrect \\fe (includes style 'encoding' "
	"field) or a damaged file.",
	"ASA supports and does character set conversions on Unicode files "
	"as well. Use \\fe0 for Unicode text."},
/* 5 SSAEC_PARSEERROR */
{3, "parse error",
	"The parser was unable to find a matching directive at the given "
	"byte position. Either the directive is unrecognized or there is "
	"junk in the script file.",
	NULL},
/* 6 SSAEC_EXC_NUMORSIGN */
{3, "expecting number or sign",
	"A number, optionally with a sign prefix, was expected at this "
	"position. However, other data was found.",
	NULL},
/* 7 SSAEC_EXC_NUM */
{3, "expecting number",
	"A number was expected at this position, but not found.",
	NULL},
/* 8 SSAEC_EXC_COLOUR */
{3, "expecting color code",
	"A color code was expected by the parser, but unrecognizable "
	"data was found.",
	NULL},
/* 9 SSAEC_EXC_DCOL */
{2, "expected ':'",
	"A double-colon (:) was expected to follow at this position.",
	NULL},
/* 10 SSAEC_EXC_AMPERSAND */
{2, "expected '&'",
	"An ampersand (&) was expected to follow at this position.",
	NULL},
/* 11 SSAEC_EXC_BRACE */
{2, "expected '('",
	"An opening brace (\"(\")) was expected to follow at this "
	"position.",
	NULL},
/* 12 SSAEC_MISS_BRACE */
{2, "missing closing brace",
	"A brace was opened, but not closed. The parser recognized "
	"this at the given position, indicating it probably to be "
	"the last possible place to close the brace.\n\n"
	"Note this might be a runaway from an earlier brace.",
	NULL},
/* 13 SSAEC_LEADWS_BRACE */
{2, "leading white space before '('",
	"White space was found before the opening brace of a style "
	"argument.\n\nThe white space is ignored by asa but might cause "
	"compatibility issues with other renderers.",
	NULL},
/* 14 SSAEC_NUM_2LONG */
{3, "number too long",
	"The number at the given position was found to be too long / "
	"too large for asa to process. Please use a smaller number.",
	NULL},
/* 15 SSAEC_NUM_INVAL */
{3, "invalid number",
	"The data at the given position failed to be parsed as "
	"number. This usually indicates junk in the script file.",
	NULL},
/* 16 SSAEC_TRUNCLINE */
{3, "truncated line",
	"More fields were expected to follow at this position, "
	"but the end of the line was found.\n\nNote this may indicate "
	"missing field separators (i.e., commas) earlier in this line.",
	NULL},
/* 17 SSAEC_TRAILGB_PARAM */
{3, "trailing garbage in parameter",
	"Additional unrecognizable data was found after the end of "
	"parameter data. This usually means the file is erroneous.",
	NULL},
/* 18 SSAEC_TRAILGB_LINE */
{3, "trailing garbage on line",
	"Additional data was found at the end of this line, but not "
	"recognized. This data might either stem from an unsupported "
	"extension or indicate extra field separators (commas) earlier "
	"on the line",
	NULL},
/* 19 SSAEC_TRAILGB_EFFECT */
{3, "trailing garbage in effect",
	"Unrecognized data was found at the end of the effect field. "
	"The effect might be invalid or an unsupported extension.",
	NULL},
/* 20 SSAEC_TRAILGB_BRACE */
{3, "trailing garbage in brace",
	"Additional unrecognizable data was found in the brace at "
	"the given position.",
	NULL},
/* 21 SSAEC_TRAILGB_CSVBRACE */
{3, "trailing garbage in csv brace",
	"Additional unrecognizable data was found in the (CSV) brace "
	"at the given position.",
	NULL},
/* 22 SSAEC_TRAILGB_TIME */
{3, "trailing garbage after time",
	"Additional unrecognizable data was found after the time code "
	"at the given position.",
	NULL},
/* 23 SSAEC_TRAILGB_ACCEL */
{3, "trailing garbage after acceleration",
	"Additional unrecognizable data was found after the "
	"acceleration value at the given position.",
	NULL},
/* 24 SSAEC_TRAILGB_COLOUR */
{3, "trailing garbage in color",
	"Additional unrecognizable data was found after the "
	"color code at the given position.",
	NULL},
/* 25 SSAEC_GB_STYLEOVER */
{2, "garbage in style override",
	"Unparseable sequences were found within a style override. "
	"('{...}')\n\nThis usually indicates (ab-)use of style override "
	"sections as comments. If this is the case, please remove the "
	"'comments' and put them in Comment: lines or start a line with ;."
	"\n\nOther possible reasons for this error are typos in style "
	"overrides / etc.",
	NULL},
/* 26 SSAEC_STYLEOVER_UNRECOG */
{3, "unrecognized style override",
	"The style override at this position is not recognized by asa. "
	"This probably indicates an unsupported extension.",
	NULL},
/* 27 SSAEC_EFFECT_UNRECOG */
{3, "unrecognized effect",
	"The effect in this line is not recognized by asa. "
	"This probably indicates an unsupported extension.",
	NULL},
/* 28 SSAEC_UNKNSTYLE */
{3, "unknown style",
	"The Style: used by this line was not defined previously. "
	"Please check for typos and/or add a style definition at the "
	"beginning of the file.",
	NULL},
/* 29 SSAEC_R0 */
{1, "problematic use of '0' style",
	"An element is referring to a style named '0'.\n\nA style with "
	"'0' as name historically stands for 'revert to line base style', "
	"Although ASA supports this notation, it is considered "
	"deprecated and should not be used anymore.",
	NULL},
/* 30 SSAEC_INVAL_TIME */
{3, "invalid time",
	"An invalid time code was encountered.",
	NULL},
/* 31 SSAEC_INVAL_ACCEL */
{3, "invalid acceleration",
	"An invalid acceleration value was encountered.",
	NULL},
/* 32 SSAEC_INVAL_ANI */
{4, "invalid animation",
	"The given style override cannot be animated because it does not "
	"make sense or is not possible.\n\nThis currently includes \\fn, "
	"\\r, \\fe, \\move and nested \\t.",
	"WARNING: use of \\fn or \\r in \\t causes the parser to bail out!"},
/* 33 SSAEC_INVAL_ENC */
{3, "invalid encoding value",
	"The encoding value used here is not known to asa. Either it is "
	"invalid or it needs to be added to asa.",
	NULL},
/* 34 SSAEC_COLOUR_STRANGE */
{0, "unusual color length",
	"A color value with a seldom used length has been found.\n\n"
	"While this is not exactly an error, it may be a typo or some bug. "
	"This message is purely informational and intended as help to spot "
	"possible logical errors.",
	NULL},
/* 35 SSAEC_COLOUR_ALPHALOST */
{0, "alpha value lost",
	"An 8-digit color value was found where a 6-digit one was "
	"expected, causing the alpha part to be dropped.\n\nThis occurs on "
	"\\c style overrides as well as on SSA 4.00 Style: lines. Use "
	"\\«num»a instead.",
	NULL},
/* 36 SSAEC_STRAY_BOM */
{0, "stray BOM",
	"The parser encountered an Unicode BOM (byte order marker) "
	"in the middle of the file.\n\nAlthough ASA skips over stray BOMs, "
	"other parsers may not do so. It is recommended to only put exactly "
	"one BOM at the very beginning of the file.",
	NULL},
/* 37 SSAVEC_NOTSUP */
{3, "command/option not supported",
	"ASA is not able to correctly process this option yet.",
	NULL},
/* 38 SSAVEC_FONTNX */
{3, "font not found",
	"The font requested by this style/override was not found."
#ifdef _WIN32
	"\n\nOn Windows this problem is primarily caused by the GetFontData "
	"interface in conjunction with copyrighted or non-TrueType fonts." 
#endif
	,
	NULL},
/* 39 SSAVEC_NOANIM */
{3, "unanimatable parameter",
	"The script is trying to animate a parameter for which no animation "
	"controller exists. Either ASA lacks the support for this or "
	"animating this element does not make sense (e.g. \\fn, \\fad).",
	NULL}
};

