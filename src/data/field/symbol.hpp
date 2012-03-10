//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2010 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_DATA_FIELD_SYMBOL
#define HEADER_DATA_FIELD_SYMBOL

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field.hpp>
#include <script/scriptable.hpp>

DECLARE_POINTER_TYPE(SymbolFilter);
DECLARE_POINTER_TYPE(SymbolVariation);

// ----------------------------------------------------------------------------- : SymbolField

DECLARE_POINTER_TYPE(SymbolField);
DECLARE_POINTER_TYPE(SymbolStyle);
#if !USE_SCRIPT_VALUE_SYMBOL
DECLARE_POINTER_TYPE(SymbolValue);
#endif

/// A field for image values
#if USE_SCRIPT_VALUE_SYMBOL
class SymbolField : public AnyField {
#else
class SymbolField : public Field {
#endif
  public:
	DECLARE_FIELD_TYPE();
	
	// no extra data
};

// ----------------------------------------------------------------------------- : SymbolStyle

/// The Style for a SymbolField
class SymbolStyle : public Style {
  public:
	inline SymbolStyle(const SymbolFieldP& field)
		: Style(field)
		, min_aspect_ratio(1), max_aspect_ratio(1)
	{}
	DECLARE_STYLE_TYPE(Symbol);
	double        min_aspect_ratio;
	double        max_aspect_ratio;	///< Bounds for the symbol's aspect ratio
	
	vector<SymbolVariationP> variations; ///< Different variantions of the same symbol
};

/// Styling for a symbol variation, defines color, border, etc.
class SymbolVariation : public IntrusivePtrBase<SymbolVariation> {
  public:
	SymbolVariation();
	~SymbolVariation();
	String        name;				///< Name of this variation
	SymbolFilterP filter;			///< Filter to color the symbol
	double        border_radius;	///< Border radius for the symbol
	
	bool operator == (const SymbolVariation&) const;
	
	DECLARE_REFLECTION();
};

// ----------------------------------------------------------------------------- : SymbolValue

#if USE_SCRIPT_VALUE_SYMBOL
typedef AnyValue SymbolValue;
typedef AnyValueP SymbolValueP;
#else
/// The Value in a SymbolField, i.e. a symbol
class SymbolValue : public Value {
  public:
	inline SymbolValue(const SymbolFieldP& field) : Value(field) {}
	DECLARE_VALUE_TYPE(Symbol, FileName);
	
	ValueType filename;    ///< Filename of the symbol (in the current package)
};
#endif

// ----------------------------------------------------------------------------- : EOF
#endif
