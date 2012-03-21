//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_RENDER_TEXT_ELEMENT
#define HEADER_RENDER_TEXT_ELEMENT

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/rotation.hpp>
#include <util/real_point.hpp>
#include <data/font.hpp>
#include <data/draw_what.hpp>

DECLARE_POINTER_TYPE(TextElement);
DECLARE_POINTER_TYPE(Font);
class TextStyle;
class Context;
class SymbolFontRef;

// ----------------------------------------------------------------------------- : TextElement

/// Information on a linebreak
enum LineBreak
{	BREAK_NO		// no line break ever
,	BREAK_MAYBE		// break here when in "direction:vertical" mode
,	BREAK_SPACE		// optional line break (' ')
,	BREAK_SOFT		// always a line break, spacing as a soft break
,	BREAK_HARD		// always a line break ('\n')
,	BREAK_LINE		// line break with a separator line (<line>)
};

/// Information on a character in a TextElement
struct CharInfo {
	RealSize  size;             ///< Size of this character
	LineBreak break_after : 31; ///< How/when to break after it?
	bool      soft : 1;         ///< Is this a 'soft' character? soft characters are ignored for alignment
	
	explicit CharInfo()
		: break_after(BREAK_NO), soft(true)
	{}
	inline CharInfo(RealSize size, LineBreak break_after, bool soft = false)
		: size(size), break_after(break_after), soft(soft)
	{}
};

/// A section of text that can be rendered using a TextViewer
class TextElement : public IntrusivePtrBase<TextElement> {
  public:
	/// What section of the input string is this element?
	size_t start, end;
	
	inline TextElement(size_t start ,size_t end) : start(start), end(end) {}
	virtual ~TextElement() {}
	
	/// Draw a subsection section of the text in the given rectangle
	/** xs give the x coordinates for each character
	 *  this->start <= start < end <= this->end <= text.size() */
	virtual void draw       (RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const = 0;
	/// Get information on all characters in the range [start...end) and store them in out
	virtual void getCharInfo(RotatedDC& dc, double scale, vector<CharInfo>& out) const = 0;
	/// Return the minimum scale factor allowed (starts at 1)
	virtual double minScale() const = 0;
	/// Return the steps the scale factor should take
	virtual double scaleStep() const = 0;
};

// ----------------------------------------------------------------------------- : TextElements

/// A list of text elements
class TextElements {
  public:
	/// Draw all the elements (as need to show the range start..end)
	void draw       (RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const;
	// Get information on all characters in the range [start...end) and store them in out
	void getCharInfo(RotatedDC& dc, double scale, size_t start, size_t end, vector<CharInfo>& out) const;
	/// Return the minimum scale factor allowed by all elements
	double minScale() const;
	/// Return the steps the scale factor should take
	double scaleStep() const;
	
	/// The actual elements
	/** They must be in order of positions and not overlap, i.e.
	 *    i < j  ==>  elements[i].end <= elements[j].start
	 */
	vector<TextElementP> elements;
	
	/// Find the element that contains the given index, if there is any
	vector<TextElementP>::const_iterator findByIndex(size_t index) const;
	
	/// Read the elements from a string
	void fromString(const String& text, size_t start, size_t end, const TextStyle& style, Context& ctx);
};

// ----------------------------------------------------------------------------- : SimpleTextElement

/// A text element that just shows text
class SimpleTextElement : public TextElement {
  public:
	SimpleTextElement(const String& content, size_t start, size_t end)
		: TextElement(start, end), content(content)
	{}
	String content;	///< Text to show
};

/// A text element that uses a normal font
class FontTextElement : public SimpleTextElement {
  public:
	FontTextElement(const String& content, size_t start, size_t end, const FontP& font, DrawWhat draw_as, LineBreak break_style)
		: SimpleTextElement(content, start, end)
		, font(font), draw_as(draw_as), break_style(break_style)
	{}
	
	virtual void draw       (RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const;
	virtual void getCharInfo(RotatedDC& dc, double scale, vector<CharInfo>& out) const;
	virtual double minScale() const;
	virtual double scaleStep() const;
  private:
	FontP     font;
	DrawWhat  draw_as;
	LineBreak break_style;
};

/// A text element that uses a symbol font
class SymbolTextElement : public SimpleTextElement {
  public:
	SymbolTextElement(const String& content, size_t start, size_t end, const SymbolFontRef& font, Context* ctx)
		: SimpleTextElement(content, start, end)
		, font(font), ctx(*ctx)
	{}
	
	virtual void draw       (RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const;
	virtual void getCharInfo(RotatedDC& dc, double scale, vector<CharInfo>& out) const;
	virtual double minScale() const;
	virtual double scaleStep() const;
  private:
	const SymbolFontRef& font; // owned by TextStyle
	Context& ctx;
};

// ----------------------------------------------------------------------------- : CompoundTextElement

/// A TextElement consisting of sub elements
class CompoundTextElement : public TextElement {
  public:
	CompoundTextElement(size_t start, size_t end) : TextElement(start, end) {}
	
	virtual void draw       (RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const;
	virtual void getCharInfo(RotatedDC& dc, double scale, vector<CharInfo>& out) const;
	virtual double minScale() const;
	virtual double scaleStep() const;
	
	TextElements elements; ///< the elements
};

/// A TextElement drawn using a grey background
class AtomTextElement : public CompoundTextElement {
  public:
	AtomTextElement(size_t start, size_t end) : CompoundTextElement(start, end) {}
	
	virtual void draw(RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const;
};

/// A TextElement drawn using a red wavy underline
class ErrorTextElement : public CompoundTextElement {
  public:
	ErrorTextElement(size_t start, size_t end) : CompoundTextElement(start, end) {}
	
	virtual void draw(RotatedDC& dc, double scale, const RealRect& rect, const double* xs, DrawWhat what, size_t start, size_t end) const;
};

// ----------------------------------------------------------------------------- : EOF
#endif
