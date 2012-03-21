//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_DATA_GRAPH_TYPE
#define HEADER_DATA_GRAPH_TYPE

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

// ----------------------------------------------------------------------------- : GraphType

/// Types of graphs
enum GraphType
{	GRAPH_TYPE_PIE
,	GRAPH_TYPE_BAR
,	GRAPH_TYPE_STACK
,	GRAPH_TYPE_SCATTER
,	GRAPH_TYPE_SCATTER_PIE
};

/// Dimensions for each graph type
inline size_t dimensionality(GraphType type) {
	if (type == GRAPH_TYPE_PIE)         return 1;
	if (type == GRAPH_TYPE_BAR)         return 1;
	if (type == GRAPH_TYPE_STACK)       return 2;
	if (type == GRAPH_TYPE_SCATTER)     return 2;
	if (type == GRAPH_TYPE_SCATTER_PIE) return 3;
	else                                return 0;
}

// ----------------------------------------------------------------------------- : EOF
#endif
