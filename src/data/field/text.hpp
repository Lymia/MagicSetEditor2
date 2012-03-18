//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2010 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_DATA_FIELD_TEXT
#define HEADER_DATA_FIELD_TEXT

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/defaultable.hpp>
#include <util/rotation.hpp>
#include <util/age.hpp>
#include <data/field.hpp>
#include <data/font.hpp>
#include <data/symbol_font.hpp>
#include <script/scriptable.hpp>
#include <script/image.hpp>
#include <gfx/gfx.hpp>

// ----------------------------------------------------------------------------- : TextField

DECLARE_POINTER_TYPE(TextField);
DECLARE_POINTER_TYPE(TextStyle);
#if !USE_SCRIPT_VALUE_TEXT
DECLARE_POINTER_TYPE(TextValue);
#endif

/// A field for values containing tagged text
#if USE_SCRIPT_VALUE_TEXT
class TextField : public AnyField {
#else
class TextField : public Field {
#endif
  public:
	TextField();
	DECLARE_FIELD_TYPE();
	
	bool multi_line;				///< Are newlines allowed in the text?
#if !USE_SCRIPT_VALUE_TEXT
	OptionalScript script;			///< Script to apply to all values
	OptionalScript default_script;	///< Script that generates the default value
	String default_name;			///< Name of "default" value
	
	virtual void initDependencies(Context&, const Dependency&) const;
#endif
};

// ----------------------------------------------------------------------------- : TextStyle

/// The Style for a TextField
class TextStyle : public Style {
  public:
	TextStyle(const TextFieldP&);
	DECLARE_STYLE_TYPE(Text);
	
	Font font;                                  ///< Font to use for the text
	SymbolFontRef symbol_font;                  ///< Symbol font for symbols in the text
	bool always_symbol;                         ///< Should everything be drawn as symbols?
	bool allow_formating;                       ///< Is formating (bold/italic/..) allowed?
	Scriptable<Alignment> alignment;            ///< Alignment inside the box
	Scriptable<double>
	    padding_left,   padding_left_min,       ///< Padding
	    padding_right,  padding_right_min,      ///< Padding
	    padding_top,    padding_top_min,        ///< Padding
	    padding_bottom, padding_bottom_min,     ///< Padding
	    line_height_soft,                       ///< Line height for soft linebreaks
	    line_height_hard,                       ///< Line height for hard linebreaks
	    line_height_line,                       ///< Line height for <line> tags
	    line_height_soft_max,                   ///< Maximum line height
	    line_height_hard_max,                   ///< Maximum line height
	    line_height_line_max,                   ///< Maximum line height
	    paragraph_height;                       ///< Fixed height of paragraphs
	Direction direction;                        ///< In what direction is text layed out?
	// information from text rendering
	double content_width, content_height;       ///< Size of the rendered text
	int    content_lines;                       ///< Number of rendered lines
	
	virtual int  update(Context&);
	virtual void initDependencies(Context&, const Dependency&) const;
	virtual void checkContentDependencies(Context&, const Dependency&) const;
	
	/// Stretch factor to use
	double getStretch() const;
};

// ----------------------------------------------------------------------------- : TextValue

#if USE_SCRIPT_VALUE_TEXT
typedef AnyValue TextValue;
typedef AnyValueP TextValueP;
#else
/// The Value in a TextField
class TextValue : public Value {
  public:
	inline TextValue(const TextFieldP& field) : Value(field) {}
	DECLARE_VALUE_TYPE(Text, Defaultable<String>);
	
	ValueType value;                ///< The text of this value
	
	virtual bool update(Context&, const Action* = nullptr);
};
#endif

// ----------------------------------------------------------------------------- : TextValue

/// A 'fake' TextValue that is used to edit some other string
/** Used by TextCtrl */
class FakeTextValue : public TextValue {
  public:
	/// Initialize the fake text value
	/** underlying can be nullptr, in that case there is no underlying value */
	FakeTextValue(const TextFieldP& field, String* underlying, bool editable, bool untagged);
	
	String* const underlying; ///< The underlying actual value, can be null
	bool const editable;      ///< The underlying value can be edited
	bool const untagged;      ///< The underlying value is untagged
	
	/// Store the value in the underlying value.
	/** May be overloaded to do some transformation */
	virtual void store();
	/// Retrieve the value from the underlying value.
	/** May be overloaded to do some transformation */
	virtual void retrieve();
	
	/// Update underlying data
	virtual void onAction(Action& a, bool undone);
	/// Editing the same underlying value?
	virtual bool equals(const Value* that);
};

// ----------------------------------------------------------------------------- : EOF
#endif
