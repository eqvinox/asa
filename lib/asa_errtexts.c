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

#define _(str) str

struct asaec_desc ssaec[ASAEC_MAX] = {
/* 0 */
{0, _("no error"),
	"",
	NULL},
/* 1 SSAEC_NOTSUP */
{3, _("command/option not supported"),
	_("Asa does not have parse support for this option/style yet.\n\n"
	"This currently includes:\n- embedded fonts\n- embedded graphics"
	"\n- drawing commands (\\p and \\clip with paint commands)"),
	NULL},
/* 2 SSAEC_UNKNOWNVER */
{3, _("unknown script version"),
	_("The asa parser was not able to recognize the given script "
	"version.\n\nCurrently supported are 'v4.00' and 'v4.00+'"),
	NULL},
/* 3 SSAEC_AMBIGUOUS_SVER */
{3, _("ambiguous script version"),
	_("Two or more script version indicators are present in this script, "
	"not all of them indicating the same version.\n\nCheck for and "
	"remove incorrect script version indicators. (this includes "
	"'[v4/v4+ Styles]')\nAsa is assuming ASS 4.00+ for this run."),
	NULL},
/* 4 SSAEC_INVAL_ENCODING */
{3, _("invalid input encoding"),
	_("An nonconvertible, probably invalid byte sequence caused "
	"character set conversion to fail.\n\nMost of the cases, this is "
	"either a result of an incorrect \\fe (includes style 'encoding' "
	"field) or a damaged file."),
	_("Asa supports and does character set conversions on Unicode files "
	"as well. Use \\fe0 for Unicode text.")},
/* 5 SSAEC_PARSEERROR */
{3, _("parse error"),
	_("The parser was unable to find a matching directive at the given "
	"byte position. Either the directive is unrecognized or there is "
	"junk in the script file."),
	NULL},
/* 6 SSAEC_EXC_NUMORSIGN */
{3, _("expecting number or sign"),
	_("A number, optionally with a sign prefix, was expected at this "
	"position. However, other data was found."),
	NULL},
/* 7 SSAEC_EXC_NUM */
{3, _("expecting number"),
	_("A number was expected at this position, but not found."),
	NULL},
/* 8 SSAEC_EXC_COLOUR */
{3, _("expecting color code"),
	_("A color code was expected by the parser, but unrecognizable "
	"data was found."),
	NULL},
/* 9 SSAEC_EXC_BOOL */
{3, _("expecting boolean value"),
	_("A boolean value, i.e. \"Yes\" or \"No\", was expected at the given "
	"position, but the value found was not recognized as either."),
	NULL},
/* 10 SSAEC_EXC_DCOL */
{2, _("expected ':'"),
	_("A double-colon (:) was expected to follow at this position."),
	NULL},
/* 11 SSAEC_EXC_AMPERSAND */
{2, _("expected '&'"),
	_("An ampersand (&) was expected to follow at this position."),
	NULL},
/* 12 SSAEC_EXC_BRACE */
{2, _("expected '('"),
	_("An opening brace (\"(\") was expected to follow at this "
	"position."),
	NULL},
/* 13 SSAEC_MISS_BRACE */
{2, _("missing closing brace"),
	_("A brace was opened, but not closed. The parser recognized "
	"this at the given position, indicating it probably to be "
	"the last possible place to close the brace.\n\n"
	"Note this might be a runaway from an earlier brace."),
	NULL},
/* 14 SSAEC_LEADWS_BRACE */
{2, _("leading white space before '('"),
	_("White space was found before the opening brace of a style "
	"argument.\n\nThe white space is ignored by asa but might cause "
	"compatibility issues with other renderers."),
	NULL},
/* 15 SSAEC_NUM_2LONG */
{3, _("number too long"),
	_("The number at the given position was found to be too long / "
	"too large for asa to process. Please use a smaller number."),
	NULL},
/* 16 SSAEC_NUM_INVAL */
{3, _("invalid number"),
	_("The data at the given position failed to be parsed as "
	"number. This usually indicates junk in the script file."),
	NULL},
/* 17 SSAEC_TRUNCLINE */
{3, _("truncated line"),
	_("More fields were expected to follow at this position, "
	"but the end of the line was found.\n\nNote this may indicate "
	"missing field separators (i.e., commas) earlier in this line."),
	NULL},
/* 18 SSAEC_TRAILGB_PARAM */
{3, _("trailing garbage in parameter"),
	_("Additional unrecognizable data was found after the end of "
	"parameter data. This usually means the file is erroneous."),
	NULL},
/* 19 SSAEC_TRAILGB_LINE */
{3, _("trailing garbage on line"),
	_("Additional data was found at the end of this line, but not "
	"recognized. This data might either stem from an unsupported "
	"extension or indicate extra field separators (commas) earlier "
	"on the line"),
	NULL},
/* 20 SSAEC_TRAILGB_EFFECT */
{3, _("trailing garbage in effect"),
	_("Unrecognized data was found at the end of the effect field. "
	"The effect might be invalid or an unsupported extension."),
	NULL},
/* 21 SSAEC_TRAILGB_BRACE */
{3, _("trailing garbage in brace"),
	_("Additional unrecognizable data was found in the brace at "
	"the given position."),
	NULL},
/* 22 SSAEC_TRAILGB_CSVBRACE */
{3, _("trailing garbage in csv brace"),
	_("Additional unrecognizable data was found in the (CSV) brace "
	"at the given position."),
	NULL},
/* 23 SSAEC_TRAILGB_TIME */
{3, _("trailing garbage after time"),
	_("Additional unrecognizable data was found after the time code "
	"at the given position."),
	NULL},
/* 24 SSAEC_TRAILGB_ACCEL */
{3, _("trailing garbage after acceleration"),
	_("Additional unrecognizable data was found after the "
	"acceleration value at the given position."),
	NULL},
/* 25 SSAEC_TRAILGB_COLOUR */
{3, _("trailing garbage in color"),
	_("Additional unrecognizable data was found after the "
	"color code at the given position."),
	NULL},
/* 26 SSAEC_GB_STYLEOVER */
{2, _("garbage in style override"),
	_("Unparseable sequences were found within a style override. "
	"('{...}')\n\nThis usually indicates (ab-)use of style override "
	"sections as comments. If this is the case, please remove the "
	"'comments' and put them in Comment: lines or start a line with ;."
	"\n\nOther possible reasons for this error are typos in style "
	"overrides / etc."),
	NULL},
/* 27 SSAEC_STYLEOVER_UNRECOG */
{3, _("unrecognized style override"),
	_("The style override at this position is not recognized by asa. "
	"This probably indicates an unsupported extension."),
	NULL},
/* 28 SSAEC_EFFECT_UNRECOG */
{3, _("unrecognized effect"),
	_("The effect in this line is not recognized by asa. "
	"This probably indicates an unsupported extension."),
	NULL},
/* 29 SSAEC_UNKNSTYLE */
{3, _("unknown style"),
	_("The Style: used by this line was not defined previously. "
	"Please check for typos and/or add a style definition at the "
	"beginning of the file."),
	NULL},
/* 30 SSAEC_R0 */
{1, _("problematic use of '0' style"),
	_("An element is referring to a style named '0'.\n\nA style with "
	"'0' as name historically stands for 'revert to line base style', "
	"Although asa supports this notation, it is considered "
	"deprecated and should not be used anymore."),
	NULL},
/* 31 SSAEC_INVAL_TIME */
{3, _("invalid time"),
	_("An invalid time code was encountered."),
	NULL},
/* 32 SSAEC_INVAL_ACCEL */
{3, _("invalid acceleration"),
	_("An invalid acceleration value was encountered."),
	NULL},
/* 33 SSAEC_INVAL_ANI */
{4, _("invalid animation"),
	_("The given style override cannot be animated because it does not "
	"make sense or is not possible.\n\nThis currently includes \\fn, "
	"\\r, \\fe, \\move and nested \\t."),
	_("WARNING: use of \\fn or \\r in \\t causes the parser to bail out!")},
/* 34 SSAEC_INVAL_ENC */
{3, _("invalid encoding value"),
	_("The encoding value used here is not known to asa. Either it is "
	"invalid or it needs to be added to asa."),
	NULL},
/* 35 SSAEC_COLOUR_STRANGE */
{0, _("unusual color length"),
	_("A color value with a seldom used length has been found.\n\n"
	"While this is not exactly an error, it may be a typo or some bug. "
	"This message is purely informational and intended as help to spot "
	"possible logical errors."),
	NULL},
/* 36 SSAEC_COLOUR_ALPHALOST */
{0, _("alpha value lost"),
	_("An 8-digit color value was found where a 6-digit one was "
	"expected, causing the alpha part to be dropped.\n\nThis occurs on "
	"\\c style overrides as well as on SSA 4.00 Style: lines. Use "
	"\\<num>a instead."),
	NULL},
/* 37 SSAEC_STRAY_BOM */
{0, _("stray BOM"),
	_("The parser encountered an Unicode BOM (byte order marker) "
	"in the middle of the file.\n\nAlthough asa skips over stray BOMs, "
	"other parsers may not do so. It is recommended to only put exactly "
	"one BOM at the very beginning of the file."),
	NULL},
/* 38 SSAEC_RAW_CTX */
{2, _("no script header"),
	_("Asa did not find a [Script info] header at the beginning of the "
	"file.\n\nThis causes asa to enter \"raw section mode\", which means "
	"it ignores Format: lines. Raw section mode is left on hitting a "
	"valid section header."),
	NULL},
/* 39 SSAEC_INVALID_CTX */
{3, _("invalid command for current section"),
	_("The command encountered is not valid for the current section.\n\n"
	"This usually means you forgot a section header, or accidentally "
	"moved a line to the wrong section. Asa tries its best to execute "
	"the command nonetheless, but this might lead to further follow-up "
	"errors."),
	NULL},
/* 40 SSAVEC_NOTSUP */
{3, _("command/option not supported"),
	_("Asa is not able to correctly process this option yet."),
	NULL},
/* 41 SSAVEC_FONTNX */
{3, _("font not found"),
#ifdef _WIN32
	_("The font requested by this style/override was not found."
	"\n\nOn Windows this problem is primarily caused by the GetFontData "
	"interface in conjunction with copyrighted or non-TrueType fonts."),
#else
	_("The font requested by this style/override was not found."),
#endif
	NULL},
/* 42 SSAVEC_NOANIM */
{3, _("unanimatable parameter"),
	_("The script is trying to animate a parameter for which no animation "
	"controller exists. Either asa lacks the support for this or "
	"animating this element does not make sense (e.g. \\fn, \\fad)."),
	NULL}
};

