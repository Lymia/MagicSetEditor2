//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_RENDER_VALUE_IMAGE
#define HEADER_RENDER_VALUE_IMAGE

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <render/value/viewer.hpp>
#include <data/field/image.hpp>

DECLARE_POINTER_TYPE(AlphaMask);

// ----------------------------------------------------------------------------- : ImageValueViewer

/// Viewer that displays an image value
class ImageValueViewer : public ValueViewer {
  public:
	DECLARE_VALUE_VIEWER(Image) : ValueViewer(parent,style) {}
	
	virtual void draw(RotatedDC& dc);
	virtual void onValueChange();
	virtual void onStyleChange(int);
			
  private:
	Bitmap bitmap; ///< Cached bitmap
	RealSize size; ///< Size of cached bitmap
	Radians angle;  ///< Angle of cached bitmap
	bool    is_default; ///< Is the default placeholder image used?
	
	/// Generate a placeholder image
	static Bitmap imagePlaceholder(const Rotation& rot, UInt w, UInt h, const Image& background, bool editing);
};

// ----------------------------------------------------------------------------- : EOF
#endif
