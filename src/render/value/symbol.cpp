//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/package.hpp>
#include <render/value/symbol.hpp>
#include <render/symbol/filter.hpp>
#include <data/symbol.hpp>
#include <gui/util.hpp> // draw_checker
#include <util/error.hpp>

DECLARE_TYPEOF_COLLECTION(SymbolVariationP);

// ----------------------------------------------------------------------------- : SymbolValueViewer

IMPLEMENT_VALUE_VIEWER(Symbol);

void SymbolValueViewer::draw(RotatedDC& dc) {
	drawFieldBorder(dc);
	// draw checker background
	draw_checker(dc, style().getInternalRect());
	double wh = min(dc.getWidth(), dc.getHeight());
	// try to load symbol
	LocalSymbolFileP symbol_file = dynamic_pointer_cast<LocalSymbolFile>(value().value);
	if (symbols.empty() && symbol_file) {
		try {
			// load symbol
			SymbolP symbol = getLocalPackage().readFile<SymbolP>(symbol_file->filename);
			// aspect ratio
			double ar = symbol->aspectRatio();
			ar = min(style().max_aspect_ratio, max(style().min_aspect_ratio, ar));
			// render and filter variations
			FOR_EACH(variation, style().variations) {
				Image img = render_symbol(symbol, *variation->filter, variation->border_radius, int(200 * ar), 200);
				Image resampled(int(wh * ar), int(wh), false);
				resample(img, resampled);
				symbols.push_back(Bitmap(resampled));
			}
		} catch (const Error& e) {
			handle_error(e);
		}
	}
	// draw image, if any
	int x = 0;
	for (size_t i = 0 ; i < symbols.size() ; ++i) {
		// todo : labels?
		dc.DrawBitmap(symbols[i], RealPoint(x, 0));
		x += symbols[i].GetWidth() + 2;
	}
}

void SymbolValueViewer::onValueChange() {
	symbols.clear();
}
