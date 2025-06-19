/*
 * Copyright 2018 Avi Halachmi (:avih) avihpit@yahoo.com https://github.com/avih
 * MIT/X Consortium License
 */

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include "st.h"
#include "boxdraw_data.h"

/* Rounded non-negative integers division of n / d  */
#define DIV(n, d) (((n) + (d) / 2) / (d))

#define SUPERSAMPLE 1

static Display *xdpy;
static Colormap xcmap;
static XftDraw *xd;
static Visual *xvis;

static void drawbox(int, int, int, int, XftColor *, XftColor *, ushort);
static void drawboxlines(int, int, int, int, XftColor *, XftColor *, ushort);

/* public API */

void
boxdraw_xinit(Display *dpy, Colormap cmap, XftDraw *draw, Visual *vis)
{
	xdpy = dpy; xcmap = cmap; xd = draw, xvis = vis;
}

int
isboxdraw(Rune u)
{
	Rune block = u & ~0xff;
	return (boxdraw && block == 0x2500 && boxdata[(uint8_t)u]) ||
	       (boxdraw_braille && block == 0x2800);
}

/* the "index" is actually the entire shape data encoded as ushort */
ushort
boxdrawindex(const Glyph *g)
{
	if (boxdraw_braille && (g->u & ~0xff) == 0x2800)
		return BRL | (uint8_t)g->u;
	if (boxdraw_bold && (g->mode & ATTR_BOLD))
		return BDB | boxdata[(uint8_t)g->u];
	return boxdata[(uint8_t)g->u];
}

void
drawboxes(int x, int y, int cw, int ch, XftColor *fg, XftColor *bg,
          const XftGlyphFontSpec *specs, int len)
{
	for ( ; len-- > 0; x += cw, specs++)
		drawbox(x, y, cw, ch, fg, bg, (ushort)specs->glyph);
}

/* implementation */

void
drawbox(int x, int y, int w, int h, XftColor *fg, XftColor *bg, ushort bd)
{
	ushort cat = bd & ~(BDB | 0xff);  /* mask out bold and data */
	if (bd & (BDL | BDA)) {
		/* lines (light/double/heavy/arcs) */
		drawboxlines(x, y, w, h, fg, bg, bd);

	} else if (cat == BBD) {
		/* lower (8-X)/8 block */
		int d = DIV((uint8_t)bd * h, 8);
		XftDrawRect(xd, fg, x, y + d, w, h - d);

	} else if (cat == BBU) {
		/* upper X/8 block */
		XftDrawRect(xd, fg, x, y, w, DIV((uint8_t)bd * h, 8));

	} else if (cat == BBL) {
		/* left X/8 block */
		XftDrawRect(xd, fg, x, y, DIV((uint8_t)bd * w, 8), h);

	} else if (cat == BBR) {
		/* right (8-X)/8 block */
		int d = DIV((uint8_t)bd * w, 8);
		XftDrawRect(xd, fg, x + d, y, w - d, h);

	} else if (cat == BBQ) {
		/* Quadrants */
		int w2 = DIV(w, 2), h2 = DIV(h, 2);
		if (bd & TL)
			XftDrawRect(xd, fg, x, y, w2, h2);
		if (bd & TR)
			XftDrawRect(xd, fg, x + w2, y, w - w2, h2);
		if (bd & BL)
			XftDrawRect(xd, fg, x, y + h2, w2, h - h2);
		if (bd & BR)
			XftDrawRect(xd, fg, x + w2, y + h2, w - w2, h - h2);

	} else if (bd & BBS) {
		/* Shades - data is 1/2/3 for 25%/50%/75% alpha, respectively */
		int d = (uint8_t)bd;
		XftColor xfc;
		XRenderColor xrc = { .alpha = 0xffff };

		xrc.red = DIV(fg->color.red * d + bg->color.red * (4 - d), 4);
		xrc.green = DIV(fg->color.green * d + bg->color.green * (4 - d), 4);
		xrc.blue = DIV(fg->color.blue * d + bg->color.blue * (4 - d), 4);

		XftColorAllocValue(xdpy, xvis, xcmap, &xrc, &xfc);
		XftDrawRect(xd, &xfc, x, y, w, h);
		XftColorFree(xdpy, xvis, xcmap, &xfc);

	} else if (cat == BRL) {
		/* braille, each data bit corresponds to one dot at 2x4 grid */
		int w1 = DIV(w, 2);
		int h1 = DIV(h, 4), h2 = DIV(h, 2), h3 = DIV(3 * h, 4);

		if (bd & 1)   XftDrawRect(xd, fg, x, y, w1, h1);
		if (bd & 2)   XftDrawRect(xd, fg, x, y + h1, w1, h2 - h1);
		if (bd & 4)   XftDrawRect(xd, fg, x, y + h2, w1, h3 - h2);
		if (bd & 8)   XftDrawRect(xd, fg, x + w1, y, w - w1, h1);
		if (bd & 16)  XftDrawRect(xd, fg, x + w1, y + h1, w - w1, h2 - h1);
		if (bd & 32)  XftDrawRect(xd, fg, x + w1, y + h2, w - w1, h3 - h2);
		if (bd & 64)  XftDrawRect(xd, fg, x, y + h3, w1, h - h3);
		if (bd & 128) XftDrawRect(xd, fg, x + w1, y + h3, w - w1, h - h3);

	}
}

void
drawboxlines(int x, int y, int w, int h, XftColor *fg, XftColor *bg, ushort bd)
{
	/* s: stem thickness. width/8 roughly matches underscore thickness. */
	/* We draw bold as 1.5 * normal-stem and at least 1px thicker.      */
	/* doubles draw at least 3px, even when w or h < 3. bold needs 6px. */
	int mwh = MIN(w, h);
	int base_s = MAX(1, DIV(mwh, 8));
	int bold = (bd & BDB) && mwh >= 6;  /* possibly ignore boldness */
	int s = bold ? MAX(base_s + 1, DIV(3 * base_s, 2)) : base_s;
	int w2 = DIV(w - s, 2), h2 = DIV(h - s, 2);
	/* the s-by-s square (x + w2, y + h2, s, s) is the center texel.    */
	/* The base length (per direction till edge) includes this square.  */

	int light = bd & (LL | LU | LR | LD);
	int diagonal = (bd & BDA) && (bd & (DR | DL));
	int double_ = bd & (DL | DU | DR | DD);

	XGCValues gcvals = {.foreground = fg->pixel, 
			    .line_width = s,
			    .line_style = LineSolid,
			    .cap_style = CapNotLast};

	Drawable drawable = XftDrawDrawable(xw.draw);

	GC gc = XCreateGC(xw.dpy, drawable,
			  GCForeground | GCLineWidth | GCLineStyle | GCCapStyle,
			  &gcvals);
	// TODO: verify correctness of diagonal lines
	if (diagonal) {

		// XRectangle clip = {.x = x, .y = y, .width = w, .height = h};
		// XSetClipRectangles(xw.dpy, gc, 0, 0, &clip, 1, Unsorted);


		if (bd & DR) {
			int down_left_x = x; 
			int down_left_y = y + h;
			int up_right_x = x + w;
			int up_right_y = y;

			XPoint points[2] = {
				{.x = down_left_x, .y = down_left_y},
				{.x = up_right_x, .y = up_right_y}
			};

			XDrawLines(xw.dpy, drawable, gc, points, 2, CoordModeOrigin);
		}

		if (bd & DL) {
			int down_right_x = x + w;
			int down_right_y = y + h;
			int up_left_x = x;
			int up_left_y = y;

			XPoint points[2] = {
				{.x = up_left_x, .y = up_left_y},
				{.x = down_right_x, .y = down_right_y},
			};

			XDrawLines(xw.dpy, drawable, gc, points, 2, CoordModeOrigin);
		}


		return;
	}

	if (light) {
		/* d: additional (negative) length to not-draw the center   */
		/* texel - at arcs and avoid drawing inside (some) doubles  */
		int is_arc = bd & BDA;
		int multi_light = light & (light - 1);
		int multi_double = double_ & (double_ - 1);
		/* light crosses double only at DH+LV, DV+LH (ref. shapes)  */
		int d = (multi_double && !multi_light) ? -s : 0;

		if (is_arc) {
			d = -mwh/2;
		}

		if (bd & LL)
			XftDrawRect(xd, fg, x, y + h2, w2 + s + d, s);
		if (bd & LU)
			XftDrawRect(xd, fg, x + w2, y, s, h2 + s + d);
		if (bd & LR)
			XftDrawRect(xd, fg, x + w2 - d, y + h2, w - w2 + d, s);
		if (bd & LD)
			XftDrawRect(xd, fg, x + w2, y + h2 - d, s, h - h2 + d);


		if (is_arc) {
			int circle_x = 0;
			int circle_y = 0;
			int angle_start = 0;
			int angle_end = 0;

			// top-right arc
			if (bd & LL && bd & LD) {
				circle_x    = x + w2 - mwh + s;
				circle_y    = y + h2;
				angle_start = 90 * 64;
				angle_end   = -90 * 64;
			}

			// bottom right arc
			if (bd & LL && bd & LU) {
				circle_x    = x + w2 - mwh + s;
				circle_y    = y + s / 2;
				angle_start = 0*64;
				angle_end   = -90*64;
			}

			// top left arc
			if (bd & LR && bd & LD) {
				circle_x    = x + w2;
				circle_y    = y + h2;
				angle_start = 180*64;
				angle_end   = -90*64;
			}

			// bottom left arc
			if (bd & LR && bd & LU) {
				circle_x    = x + w2;
				circle_y    = y + s / 2;
				angle_start = 270*64;
				angle_end   = -90*64;
			}

			// angle_start = 0;
			// angle_end = 360 * 64;

			int hi_s = s * SUPERSAMPLE;
			XGCValues hi_gcvals = {
				.foreground = fg->pixel, 
				.background = bg->pixel,
				.line_width = hi_s,
				.line_style = LineSolid,
				.cap_style = CapNotLast};


			GC hi_gc = XCreateGC(xw.dpy, drawable,
					  GCForeground | GCLineWidth | GCLineStyle | GCCapStyle,
					  &hi_gcvals);
			Pixmap hi_res = XCreatePixmap(xdpy, drawable,
				 mwh * SUPERSAMPLE, mwh * SUPERSAMPLE,
				 DefaultDepth(xdpy, DefaultScreen(xdpy)));


			XftColor xfc;
			XRenderColor xrc = { .alpha = 0 };
			xrc.red = fg->color.red;
			xrc.green = fg->color.green;
			xrc.blue = fg->color.blue;

			XftColorAllocValue(xdpy, xvis, xcmap, &xrc, &xfc);
			XftDrawRect(xd, &xfc, 0, 0,
				mwh * SUPERSAMPLE, mwh * SUPERSAMPLE);
			XftColorFree(xdpy, xvis, xcmap, &xfc);

;
			XDrawArc(xdpy, hi_res, hi_gc,
				hi_s / 2, hi_s / 2,
				mwh * SUPERSAMPLE - hi_s , mwh * SUPERSAMPLE - hi_s ,
				angle_start, angle_end);

			XImage * hi_img = XGetImage(xdpy, hi_res,
				0, 0, mwh * SUPERSAMPLE, mwh * SUPERSAMPLE,
				AllPlanes, ZPixmap);

			XImage *lo_img = XGetImage(xdpy, drawable, 
				circle_x, circle_y, mwh, mwh,
				AllPlanes, ZPixmap);


			int bytes_per_pixel = hi_img->bits_per_pixel / 8;
			int lo_img_size = lo_img->height * lo_img->bytes_per_line;

			// Downsample using simple box filter
			for (int y = 0; y < mwh; ++y) {
			    for (int x = 0; x < mwh; ++x) {
				unsigned long r = 0, g = 0, b = 0;

				for (int dy = 0; dy < SUPERSAMPLE; ++dy) {
				    for (int dx = 0; dx < SUPERSAMPLE; ++dx) {
					int hx = x * SUPERSAMPLE + dx;
					int hy = y * SUPERSAMPLE + dy;

					unsigned long pixel = XGetPixel(hi_img, hx, hy);

					r += (pixel >> 16) & 0xff;
					g += (pixel >> 8) & 0xff;
					b += pixel & 0xff;
				    }
				}

				r /= (SUPERSAMPLE * SUPERSAMPLE);
				g /= (SUPERSAMPLE * SUPERSAMPLE);
				b /= (SUPERSAMPLE * SUPERSAMPLE);

				unsigned long avg_pixel =
				    ((r & 0xff) << 16) |
				    ((g & 0xff) << 8) |
				    (b & 0xff);

				// FIX: if the average pixel is 0, we skip it
				if (avg_pixel == 0) {
					continue;
				}

				XPutPixel(lo_img, x, y, avg_pixel);
			    }
			}

			XPutImage(xdpy, drawable, gc, lo_img,
				  0, 0,         // src x/y
				  circle_x, circle_y,  // dest x/y
				  mwh, mwh);

			XDestroyImage(hi_img);  // Frees data too
			XDestroyImage(lo_img);  // Frees data + malloc'd buffer
			XFreePixmap(xdpy, hi_res);
			XFreeGC(xw.dpy, hi_gc);

		}

	}

	/* double lines - also align with light to form heavy when combined */
	if (double_) {
		/*
		* going clockwise, for each double-ray: p is additional length
		* to the single-ray nearer to the previous direction, and n to
		* the next. p and n adjust from the base length to lengths
		* which consider other doubles - shorter to avoid intersections
		* (p, n), or longer to draw the far-corner texel (n).
		*/
		int dl = bd & DL, du = bd & DU, dr = bd & DR, dd = bd & DD;
		if (dl) {
			int p = dd ? -s : 0, n = du ? -s : dd ? s : 0;
			XftDrawRect(xd, fg, x, y + h2 + s, w2 + s + p, s);
			XftDrawRect(xd, fg, x, y + h2 - s, w2 + s + n, s);
		}
		if (du) {
			int p = dl ? -s : 0, n = dr ? -s : dl ? s : 0;
			XftDrawRect(xd, fg, x + w2 - s, y, s, h2 + s + p);
			XftDrawRect(xd, fg, x + w2 + s, y, s, h2 + s + n);
		}
		if (dr) {
			int p = du ? -s : 0, n = dd ? -s : du ? s : 0;
			XftDrawRect(xd, fg, x + w2 - p, y + h2 - s, w - w2 + p, s);
			XftDrawRect(xd, fg, x + w2 - n, y + h2 + s, w - w2 + n, s);
		}
		if (dd) {
			int p = dr ? -s : 0, n = dl ? -s : dr ? s : 0;
			XftDrawRect(xd, fg, x + w2 + s, y + h2 - p, s, h - h2 + p);
			XftDrawRect(xd, fg, x + w2 - s, y + h2 - n, s, h - h2 + n);
		}
	}

	XFreeGC(xw.dpy, gc);
}
