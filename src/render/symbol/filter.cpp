//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <render/symbol/filter.hpp>
#include <render/symbol/viewer.hpp>
#include <gfx/gfx.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : Symbol filtering

void filter_symbol(Image& symbol, const SymbolFilter& filter) {
	Byte* data  = symbol.GetData();
	Byte* alpha = symbol.GetAlpha();
	UInt width = symbol.GetWidth(), height = symbol.GetHeight();
	// HACK: wxGTK seems to fail sometimes if you ask it to allocate the alpha channel.
	//       This manually allocates the memory and gives it to the image to handle.
	if (!alpha) {
		alpha = (Byte*) malloc(width * height);
		symbol.SetAlpha(alpha);
	}
	for (UInt y = 0 ; y < height ; ++y) {
		for (UInt x = 0 ; x < width ; ++x) {
			// Determine set
			//  green           -> border or outside
			//  green+red=white -> border
			if (data[0] != data[2]) {
				// yellow/blue = editing hint, leave alone
			} else {
				SymbolSet point = data[1] ? (data[0] ? SYMBOL_BORDER : SYMBOL_OUTSIDE) : SYMBOL_INSIDE;
				// Call filter
				AColor result = filter.color((double)x / width, (double)y / height, point);
				// Store color
				data[0]  = result.Red();
				data[1]  = result.Green();
				data[2]  = result.Blue();
				alpha[0] = result.alpha;
			}
			// next
			data  += 3;
			alpha += 1;
		}
	}
}

Image render_symbol(const SymbolP& symbol, const SymbolFilter& filter, double border_radius, int width, int height, bool edit_hints, bool allow_smaller) {
	Image i = render_symbol(symbol, border_radius, width, height, edit_hints, allow_smaller);
	filter_symbol(i, filter);
	return i;
}

// ----------------------------------------------------------------------------- : SymbolFilter

IMPLEMENT_REFLECTION_NO_SCRIPT(SymbolFilter) {
	REFLECT_IF_NOT_READING {
		String fill_type = fillType();
		REFLECT(fill_type);
	}
}
template <> void GetMember::handle(const intrusive_ptr<SymbolFilter>& f) {
	handle(*f);
}

template <>
intrusive_ptr<SymbolFilter> read_new<SymbolFilter>(Reader& reader) {
	// there must be a fill type specified
	String fill_type;
	reader.handle(_("fill_type"), fill_type);
	if      (fill_type == _("solid"))			return intrusive(new SolidFillSymbolFilter);
	else if (fill_type == _("linear gradient"))	return intrusive(new LinearGradientSymbolFilter);
	else if (fill_type == _("radial gradient"))	return intrusive(new RadialGradientSymbolFilter);
	else if (fill_type.empty()) {
		reader.warning(_ERROR_1_("expected key", _("fill type")));
		throw ParseError(_ERROR_("aborting parsing"));
	} else {
		reader.warning(_ERROR_1_("unsupported fill type", fill_type));
		throw ParseError(_ERROR_("aborting parsing"));
	}
}

// ----------------------------------------------------------------------------- : SolidFillSymbolFilter

String SolidFillSymbolFilter::fillType() const { return _("solid"); }

AColor SolidFillSymbolFilter::color(double x, double y, SymbolSet point) const {
	if      (point == SYMBOL_INSIDE) return fill_color;
	else if (point == SYMBOL_BORDER) return border_color;
	else                             return AColor(0,0,0,0);
}

bool SolidFillSymbolFilter::operator == (const SymbolFilter& that) const {
	const SolidFillSymbolFilter* that2 = dynamic_cast<const SolidFillSymbolFilter*>(&that);
	return that2 && fill_color   == that2->fill_color
	             && border_color == that2->border_color;
}

IMPLEMENT_REFLECTION(SolidFillSymbolFilter) {
	REFLECT_BASE(SymbolFilter);
	REFLECT(fill_color);
	REFLECT(border_color);
}

// ----------------------------------------------------------------------------- : GradientSymbolFilter

template <typename T>
AColor GradientSymbolFilter::color(double x, double y, SymbolSet point, const T* t) const {
	if      (point == SYMBOL_INSIDE) return lerp(fill_color_1,   fill_color_2,   t->t(x,y));
	else if (point == SYMBOL_BORDER) return lerp(border_color_1, border_color_2, t->t(x,y));
	else                             return AColor(0,0,0,0);
}

bool GradientSymbolFilter::equal(const GradientSymbolFilter& that) const {
	return fill_color_1   == that.fill_color_1
	    && fill_color_2   == that.fill_color_2
	    && border_color_1 == that.border_color_1
	    && border_color_2 == that.border_color_2;
}

IMPLEMENT_REFLECTION(GradientSymbolFilter) {
	REFLECT_BASE(SymbolFilter);
	REFLECT(fill_color_1);
	REFLECT(fill_color_2);
	REFLECT(border_color_1);
	REFLECT(border_color_2);
}

// ----------------------------------------------------------------------------- : LinearGradientSymbolFilter

// TODO: move to some general util header
inline double sqr(double x) { return x * x; }

String LinearGradientSymbolFilter::fillType() const { return _("linear gradient"); }

LinearGradientSymbolFilter::LinearGradientSymbolFilter()
	: center_x(0.5), center_y(0.5)
	, end_x(1), end_y(1)
{}
LinearGradientSymbolFilter::LinearGradientSymbolFilter
		( const Color& fill_color_1, const Color& border_color_1
		, const Color& fill_color_2, const Color& border_color_2
		, double center_x, double center_y, double end_x, double end_y)
	: GradientSymbolFilter(fill_color_1, border_color_1, fill_color_2, border_color_2)
	, center_x(center_x), center_y(center_y)
	, end_x(end_x), end_y(end_y)
{}

AColor LinearGradientSymbolFilter::color(double x, double y, SymbolSet point) const {
	len = sqr(end_x - center_x) + sqr(end_y - center_y);
	if (len == 0) len = 1; // prevent div by 0
	return GradientSymbolFilter::color(x,y,point,this);
}

double LinearGradientSymbolFilter::t(double x, double y) const {
	double t= fabs( (x - center_x) * (end_x - center_x) + (y - center_y) * (end_y - center_y)) / len;
	return min(1.,max(0.,t));
}

bool LinearGradientSymbolFilter::operator == (const SymbolFilter& that) const {
	const LinearGradientSymbolFilter* that2 = dynamic_cast<const LinearGradientSymbolFilter*>(&that);
	return that2 && equal(*that2)
	             && center_x == that2->center_x && end_x == that2->end_x
	             && center_y == that2->center_y && end_y == that2->end_y;
}

IMPLEMENT_REFLECTION(LinearGradientSymbolFilter) {
	REFLECT_BASE(GradientSymbolFilter);
	REFLECT(center_x); REFLECT(center_y);
	REFLECT(end_x);    REFLECT(end_y);
}

// ----------------------------------------------------------------------------- : RadialGradientSymbolFilter

String RadialGradientSymbolFilter::fillType() const { return _("radial gradient"); }

AColor RadialGradientSymbolFilter::color(double x, double y, SymbolSet point) const {
	return GradientSymbolFilter::color(x,y,point,this);
}

double RadialGradientSymbolFilter::t(double x, double y) const {
	return sqrt( (sqr(x - 0.5) + sqr(y - 0.5)) * 2); 
}

bool RadialGradientSymbolFilter::operator == (const SymbolFilter& that) const {
	const RadialGradientSymbolFilter* that2 = dynamic_cast<const RadialGradientSymbolFilter*>(&that);
	return that2 && equal(*that2);
}
