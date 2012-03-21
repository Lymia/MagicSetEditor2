//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/gfx.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : Linear Blend

// sqr(x) = x^2
template <typename T> inline T sqr(T x) { return x * x; }

void linear_blend(Image& img1, const Image& img2, double x1,double y1, double x2,double y2) {
	int width = img1.GetWidth(), height = img1.GetHeight();
	if (img2.GetWidth() != width || img2.GetHeight() != height) {
		throw Error(_ERROR_("images used for blending must have the same size"));
	}
	
	const int fixed = 1<<16; // fixed point multiplier
	// equation:
	//   x * xm + y * ym + d  ==  fixed * f(x,y)
	// xm and ym are multiples of delta x/y:
	//   xm = a w (x2-x1);  ym = a h (y2-y1)
	// known values
	//   f(x1*w, y1*h) = 0
	//   f(x2*w, y2*h) = 1
	// filling in:
	//   x1 * w * a * w * (x2-x1)  +  y1 * h * a * h * (y2-y1)  +  d  ==  0
	//   x2 * w * a * w * (x2-x1)  +  y2 * h * a * h * (y2-y1)  +  d  ==  fixed
	// solving for a and d:
	//   (using dx = x1-x2, dy = y1-y2)
	//   a = fixed / (w^2 dx^2 + h^2 dy^2)
	//   d = a * (w^2 x1 dx + h^2 y1 dy)
	if (x1==x2 && y1==y2) throw Error(_ERROR_("coordinates for blending overlap"));
	double a = fixed / (sqr(width) * sqr(x1-x2)  +  sqr(height) * sqr(y1-y2));
	int xm = to_int( (x2 - x1) * width  * a );
	int ym = to_int( (y2 - y1) * height * a );
	int d  = to_int( - (x1 * width * xm + y1 * height * ym) );
	
	Byte *data1 = img1.GetData(), *data2 = img2.GetData();
	// blend pixels
	for (int y = 0 ; y < height ; ++y) {
		for (int x = 0 ; x < width ; ++x) {
			int mult = x * xm + y * ym + d;
			if (mult < 0)      mult = 0;
			if (mult > fixed)  mult = fixed;
			data1[0] = data1[0] + mult * (data2[0] - data1[0]) / fixed;
			data1[1] = data1[1] + mult * (data2[1] - data1[1]) / fixed;
			data1[2] = data1[2] + mult * (data2[2] - data1[2]) / fixed;
			data1 += 3;
			data2 += 3;
		}
	}
}

// ----------------------------------------------------------------------------- : Mask Blend

void mask_blend(Image& img1, const Image& img2, const Image& mask) {
	if (img2.GetWidth() != img1.GetWidth() || img2.GetHeight() != img1.GetHeight()
	 || mask.GetWidth() != img1.GetWidth() || mask.GetHeight() != img1.GetHeight()) {
		throw Error(_("Images used for blending must have the same size"));
	}
	
	UInt size = img1.GetWidth() * img1.GetHeight() * 3;
	Byte *data1 = img1.GetData(), *data2 = img2.GetData(), *dataM = mask.GetData();
	// for each subpixel...
	for (UInt i = 0 ; i < size ; ++i) {
		data1[i] = (data1[i] * dataM[i] + data2[i] * (255 - dataM[i])) / 255;
	}
}

// ----------------------------------------------------------------------------- : Alpha

void set_alpha(Image& img, const Image& img_alpha) {
	Image img_alpha_resampled = resample(img_alpha, img.GetWidth(), img.GetHeight());
	if (!img.HasAlpha()) img.InitAlpha();
	Byte *im = img.GetAlpha(), *al = img_alpha_resampled.GetData();
	size_t size = img.GetWidth() * img.GetHeight();
	for (size_t i = 0 ; i < size ; ++i) {
		im[i] = (im[i] * al[i*3]) / 255;
	}
}

void set_alpha(Image& img, Byte* al, const wxSize& alpha_size) {
	if (img.GetWidth() != alpha_size.GetWidth() || img.GetHeight() != alpha_size.GetHeight()) {
		throw Error(_("Image must have same size as mask"));
	}
	if (!img.HasAlpha()) {
		// copy
		img.InitAlpha();
		memcpy(img.GetAlpha(), al, img.GetWidth() * img.GetHeight());
	} else{
		// merge
		Byte *im = img.GetAlpha();
		size_t size = img.GetWidth() * img.GetHeight();
		for (size_t i = 0 ; i < size ; ++i) {
			im[i] = (im[i] * al[i]) / 255;
		}
	}
}

void set_alpha(Image& img, double alpha) {
	Byte b_alpha = Byte(alpha * 255);
	if (!img.HasAlpha()) {
		img.InitAlpha();
		memset(img.GetAlpha(), b_alpha, img.GetWidth() * img.GetHeight());
	} else {
		Byte *im = img.GetAlpha();
		size_t size = img.GetWidth() * img.GetHeight();
		for (size_t i = 0 ; i < size ; ++i) {
			im[i] = (im[i] * b_alpha) / 255;
		}
	}
}
