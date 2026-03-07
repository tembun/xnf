/*
 * xnf -- X11 notification program.
 *
 * Inspired by https://git.sr.ht/~crm/nfy/
 */

#include <sys/file.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include "config.h"

#define MAX_LN 256
#define MAX_LNS 256

const char *lock_file = "/tmp/.xnf.lock";

struct ln {
	char str[MAX_LN+1];
	int asc;
};

void *scalloc(size_t n, size_t size);
void usage(void);
size_t utf8len(const char *s);

void *
scalloc(size_t n, size_t size)
{
	void *ret;

	if ((ret = calloc(n, size)) == NULL)
		errx(1, "Can't calloc() %zu objects %zu bytes each", n, size);
	return (ret);
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: xnf message [tl | tc | tr | bl | bc | br | cl | cc | cr]\n");
	exit(2);
}

size_t
utf8len(const char *s)
{
	size_t cnt;

	cnt = 0;
	while (*s)
		cnt += (*s++ & 0xC0) != 0x80;
	return (cnt);
}

int
main(int argc, char **argv)
{
	Colormap colormap;
	Display *dpl;
	Screen *screenp;
	Visual *vis;
	Window w;
	XColor color;
	XEvent xev;
	XGlyphInfo glyph;
	XSetWindowAttributes wattrs = {0};
	XftColor *fontcolor;
	XftDraw *drw;
	XftFont *font;
	char pesc;
	int i, j;
	int lns_l;
	int lock_fd;
	int screen;
	int screen_w, screen_h;
	int w_x, w_y, w_w, w_h;
	pid_t pid;
	struct ln lns[MAX_LNS];

	if (argc == 1 || argc > 3)
		usage();

	j = 0;
	lns_l = 0;
	pesc = 0;
	/*
	 * Split message string into lines
	 */
	for (i = 0; argv[1][i] != '\0'; i++) {
		switch (argv[1][i]) {
		case 'n':
			if (pesc == 1) {	/* Means \n */
				pesc = 0;
				lns[lns_l++].str[j] = '\0';
				j = 0;
				continue;
			}
			/* FALLTHROUGH */
		default:
			if (argv[1][i] == '\\') {
				pesc ^= 1;
				if (pesc == 1)
					break;
			}
			pesc = 0;
			lns[lns_l].str[j++] = argv[1][i];
			if (j == MAX_LN)
				lns[lns_l].str[j] = '\0';
		}
	}
	lns[lns_l++].str[j] = '\0';

	/*
	 * Fork, detach from the terminal and own or wait for the advisory lock.
	 */
	if ((pid = fork()) == -1)
		err(1, "fork()");
	if (pid != 0) /* Terminate the parent */
		exit(0);
	if (setsid() == -1)
		err(1, "setsid()");
	if ((lock_fd = open(lock_file, O_CREAT | O_WRONLY, 0600)) == -1)
		err(1, "open()");
	if (flock(lock_fd, LOCK_EX) == -1)
		err(1, "flock()");

	/*
	 * Initialize X11 variables and structures
	 */
	if ((dpl = XOpenDisplay(NULL)) == NULL)
		err(1, "XOpenDisplay()");
	screen = DefaultScreen(dpl);
	screenp = DefaultScreenOfDisplay(dpl);
	screen_w = WidthOfScreen(screenp);
	screen_h = HeightOfScreen(screenp);
	vis = DefaultVisual(dpl, screen);
	colormap = DefaultColormap(dpl, screen);
	fontcolor = scalloc(1, sizeof(XftColor));
	if (!XftColorAllocName(dpl, DefaultVisual(dpl, screen),
	    DefaultColormap(dpl, screen), font_color, fontcolor))
		err(1, "XftColorAllocName()");

	/*
	 * Set window attributes
	 */
	wattrs.override_redirect = True;
	XAllocNamedColor(dpl, colormap, bg_color, &color, &color);
	wattrs.background_pixel = color.pixel;
	XAllocNamedColor(dpl, colormap, border_color, &color, &color);
	wattrs.border_pixel = color.pixel;
	if ((font = XftFontOpenName(dpl, screen, fontname)) == NULL)
		err(1, "XftFontOpenName(%s)", fontname);

	/*
	 * Compute window size
	 */
	w_w = 0;
	w_h = padding * 2 + (lns_l - 1) * line_gap;
	for (i = 0; i < lns_l; i++) {
		XftTextExtents8(dpl, font, (unsigned char *)lns[i].str,
		    utf8len(lns[i].str), &glyph);
		if (glyph.width > w_w)
			w_w = glyph.width;
		lns[i].asc = font->ascent;
		w_h += glyph.height;
	}
	w_w += padding * 2;
	w_y = 0;
	w_x = screen_w - w_w - border_sz * 2;
	if (argc == 3) {
		switch (argv[2][0]) {
		case 'b':
			w_y = screen_h - w_h - border_sz * 2;
			break;
		case 'c':
			w_y = screen_h / 2 - (w_h / 2) - border_sz;
			break;
		case 't':
			w_y = 0;
			break;
		default:
			usage();
		}
		switch(argv[2][1]) {
		case 'c':
			w_x = screen_w / 2 - (w_w / 2) - border_sz;
			break;
		case 'l':
			w_x = 0;
			break;
		case 'r':
			w_x = screen_w - w_w - border_sz * 2;
			break;
		default:
			usage();
		}
	}

	/*
	 * Create and map a window
	 */
	w = XCreateWindow(dpl, DefaultRootWindow(dpl), w_x, w_y,
	    w_w, w_h, border_sz, DefaultDepth(dpl, screen), CopyFromParent,
	    vis, CWOverrideRedirect | CWBackPixel | CWBorderPixel, &wattrs);
	drw = XftDrawCreate(dpl, w, vis, colormap);
	XSelectInput(dpl, w, ExposureMask | ButtonPress);
	XMapWindow(dpl, w);
	XSync(dpl, False);

	for (;;) {
		XNextEvent(dpl, &xev);
		switch (xev.type) {
		case Expose:
			XClearWindow(dpl, w);
			j = 0;
			for (i = 0; i < lns_l; i++)
				XftDrawStringUtf8(drw, fontcolor, font, padding, padding +
				    line_gap * i + (j += lns[i].asc), (unsigned char *)lns[i].str,
				    strlen(lns[i].str));
			break;
		case ButtonPress:
			goto end;
		}
	}
end:
	free(fontcolor);
	XCloseDisplay(dpl);
	close(lock_fd);
	return (0);
}
