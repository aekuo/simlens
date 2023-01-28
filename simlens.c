#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

enum
{
	DrawImg = 1,
	DrawObj = 2,
	DrawLines = 4,
	DrawCont = 8,
	DrawDots = 16,
};

static char *buttons[] =
{
	"undo",
	"clear",
	"",
	"lines",
	"contour",
	"dots",
	"image",
	"object",
	"",
	"exit",
	0
};

Menu menu =
{
	buttons
};

typedef struct { // mb mozhno bilo i bez etogo
	Point p;
	int t;
} Point_t;

static int flags, fr, fv, er, ev;
static const int foc = 100;
static Point obj[128], c;
static Point_t img[128];
Image *back, *axis;
Image *dots, *virt, *real;

static Point_t
refract(Point p) 
{
	Point_t np;
	static int x, y;
	static float f, h;

	x = c.x - p.x;
	if (abs(x) == foc || x == 0) return np;
	if (abs(x) > foc)
		np.t = 1;
	else
		np.t = 0;
	y = c.y - p.y;
	f = foc * x  / (abs(x) - foc);
	h = (f / x) * y;
	np.p.x = f + c.x;
	np.p.y = h + c.y;
	return np;
}

static void
drawaxis(void)
{
	static Rectangle r;
	static Point p, f;

	r = screen->r;

	p.x = r.max.x - 10;
	p.y = c.y;
	line(screen, c, p, 0, 0, 0, axis, ZP);
	p.x = r.min.x + 10;
	line(screen, c, p, 0, 0, 0, axis, ZP);

	p.x = c.x;
	p.y = r.max.y - 10;
	line(screen, c, p, 0, Endarrow, 0, axis, ZP);
	p.y = r.min.y + 10;
	line(screen, c, p, 0, Endarrow, 0, axis, ZP);
	
	f.y = c.y;

	f.x = c.x + foc;
	p.x = f.x;
	p.y = f.y + 30;
	line(screen, f, p, 0, 0, 0, axis, ZP);
	p.y = f.y - 30;
	line(screen, f, p, 0, 0, 0, axis, ZP);

	f.x = c.x - foc;
	p.x = f.x;
	p.y = f.y + 30;
	line(screen, f, p, 0, 0, 0, axis, ZP);
	p.y = f.y - 30;
	line(screen, f, p, 0, 0, 0, axis, ZP);
}

// known bugs: kogda perehodim za focus, posle tret'ei tochki figura obrezaetsya
// TODO: refactor nado etoy dichi a dal'she mne len'
static void
drawdots(void)
{
	static Point p;
	static int i;

	for (i = 0; i < 128; i++) {
		if (obj[i].x == 0 && obj[i].y == 0) {
			if (flags & DrawCont) {
				if (flags & DrawObj)
					line(screen, obj[i-1], obj[0], 0, 0, 0, dots, ZP);
				if (flags & DrawImg)
					if (img[i-1].t) // po idee gde to tut bug (bug logiki vsego wtf)
						line(screen, img[i-1].p, img[fr].p, 0, 0, 0, real, ZP);
					else
						line(screen, img[i-1].p, img[fv].p, 0, 0, 0, virt, ZP);
			}
			break;
		}
		if (flags & DrawObj) {
			if (flags & DrawDots) {
				fillellipse(screen, obj[i], 3, 3, dots, ZP);
			}
			if (flags & DrawCont) {
				if (i) line(screen, obj[i-1], obj[i], 0, 0, 0, dots, ZP);
			}		
		}
		img[i] = refract(obj[i]);
		if (!er && img[i].t) { // seychas 3 chasa utra ya khz nakhuya ya eto sdelal
			fr = i;
			er = 1;
		}
		if (!ev && !img[i].t) {
			fv = i;
			ev = 1;
		}
		p.x = c.x;
		p.y = obj[i].y;
		if (flags & DrawImg) {
			if (flags & DrawDots) {
				fillellipse(screen, img[i].p, 3, 3, img[i].t ? real : virt, ZP);
			}
			if (flags & DrawLines && flags & DrawObj) {
				line(screen, obj[i], img[i].p, 0, 0, 0, img[i].t ? real : virt, ZP);
				line(screen, obj[i], p, 0, 0, 0, img[i].t ? real : virt, ZP);
				line(screen, p, img[i].p, 0, 0, 0, img[i].t ? real : virt, ZP);
			}
			if (flags & DrawCont && i) {
				if (img[i-1].t == img[i].t)
					line(screen, img[i-1].p, img[i].p, 0, 0, 0, img[i].t ? real : virt, ZP);
			}
		}
	}
}

static void
redraw(void)
{
	static Rectangle r;

	r = screen->r;
	c = divpt(addpt(r.min, r.max), 2);
	draw(screen, r, back, nil, ZP);
	drawaxis();
	drawdots();
	
	flushimage(display, 1);
}

static void
clear(void)
{
	static int i;

	for (i = 0; i < 128; i++) {
		obj[i] = subpt(obj[i], obj[i]);
		img[i].p = subpt(img[i].p, img[i].p);
		img[i].t = 0;
	}
	fv = fr = er = ev = 0;
	redraw();
}

void
eresized(int new)
{
	if (new && getwindow(display, Refnone) < 0)
		sysfatal("resize failed: %r");
	redraw();
}

/* simlens: thin (convex) lens simulator */
void
main(void)
{
	Mouse m;
	int n;

	if (initdraw(nil, nil, "simlens") < 0)
		sysfatal("initdraw failed: %r");
	back = allocimagemix(display, DPaleyellow, DWhite);
	axis = allocimage(display, Rect(0,0,1,1), RGB24, 1, DBlack); // blya tak navernoe tozhe ne stoit delat'
	dots = allocimage(display, Rect(0,0,1,1), RGB24, 1, DRed);   // no niche, uchimsya !
	virt = allocimage(display, Rect(0,0,1,1), RGB24, 1, DGreen);
	real = allocimage(display, Rect(0,0,1,1), RGB24, 1, DBlue);
	redraw();
	einit(Emouse);
	n = 0;
	flags = DrawImg|DrawObj|DrawDots;
	for (;;) {
		m = emouse();
		switch (m.buttons) {
		case 1:
			obj[n++] = drawrepl(screen->r, m.xy);
			if (n == 128) n = 0;
			redraw();
			break;
		case 4:
			switch (emenuhit(3, &m, &menu)) {
			case 0:
				obj[--n] = subpt(obj[n], obj[n]);
				redraw();
				break;
			case 1:
				n = 0;
				clear();
				break;
			case 3:
				flags ^= DrawLines;
				redraw();
				break;
			case 4:
				flags ^= DrawCont;
				redraw();
				break;
			case 5:
				flags ^= DrawDots;
				redraw();
				break;
			case 6:
				flags ^= DrawImg;
				redraw();
				break;
			case 7:
				flags ^= DrawObj;
				redraw();
				break;
			case 9:
				exits(nil);
			}
		}
	}
}
