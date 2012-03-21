//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_UTIL_ALIGNMENT
#define HEADER_UTIL_ALIGNMENT

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/real_point.hpp>

// ----------------------------------------------------------------------------- : Alignment

// Alignment in a textbox, specifies both horizontal and vertical alignment
enum Alignment
// horizontal
{	ALIGN_LEFT				= 0x01
,	ALIGN_CENTER			= 0x02
,	ALIGN_RIGHT				= 0x04
,	ALIGN_HORIZONTAL		= ALIGN_LEFT | ALIGN_CENTER | ALIGN_RIGHT
// horizontal filling
,	ALIGN_STRETCH			= 0x10
,	ALIGN_JUSTIFY_WORDS		= 0x20
,	ALIGN_JUSTIFY_ALL		= 0x40
,	ALIGN_FILL				= ALIGN_STRETCH | ALIGN_JUSTIFY_WORDS | ALIGN_JUSTIFY_ALL
// horizontal fill modifiers
,	ALIGN_IF_OVERFLOW		= 0x1000 // only fill if text_width > box_width
,	ALIGN_IF_SOFTBREAK		= 0x2000 // only fill before soft line breaks
// vertical
,	ALIGN_TOP				= 0x100
,	ALIGN_MIDDLE			= 0x200
,	ALIGN_BOTTOM			= 0x400
,	ALIGN_VERTICAL			= ALIGN_TOP | ALIGN_MIDDLE | ALIGN_BOTTOM
// modifiers
// common combinations
,	ALIGN_TOP_LEFT			= ALIGN_TOP    | ALIGN_LEFT
,	ALIGN_TOP_CENTER		= ALIGN_TOP    | ALIGN_CENTER
,	ALIGN_TOP_RIGHT			= ALIGN_TOP    | ALIGN_RIGHT
,	ALIGN_MIDDLE_LEFT		= ALIGN_MIDDLE | ALIGN_LEFT
,	ALIGN_MIDDLE_CENTER		= ALIGN_MIDDLE | ALIGN_CENTER
,	ALIGN_MIDDLE_RIGHT		= ALIGN_MIDDLE | ALIGN_RIGHT
};


/// How much should an object with obj_width be moved to be aligned in a box with box_width?
double align_delta_x(Alignment align, double box_width, double obj_width);

/// How much should an object with obj_height be moved to be aligned in a box with box_height?
double align_delta_y(Alignment align, double box_height, double obj_height);

/// Align a rectangle inside another rectangle
/** returns the topleft coordinates of the inner rectangle after alignment
 */
RealPoint align_in_rect(Alignment align, const RealSize& to_align, const RealRect& outer);

// ----------------------------------------------------------------------------- : Direction

/// Direction to place something in
enum Direction {
	LEFT_TO_RIGHT, RIGHT_TO_LEFT,
	TOP_TO_BOTTOM, BOTTOM_TO_TOP,
	BOTTOM_RIGHT_TO_TOP_LEFT,
	TOP_LEFT_TO_BOTTOM_RIGHT,
	BOTTOM_LEFT_TO_TOP_RIGHT,
	TOP_RIGHT_TO_BOTTOM_LEFT,
	HORIZONTAL = LEFT_TO_RIGHT,
	VERTICAL   = TOP_TO_BOTTOM
};

/// Move a point in a direction
/** If the direction is horizontal the to_move.width is used, otherwise to_move.height */
RealPoint move_in_direction(Direction dir, const RealPoint& point, const RealSize to_move, double spacing = 0);

// ----------------------------------------------------------------------------- : EOF
#endif
