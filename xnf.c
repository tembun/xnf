/*
 * xnf -- X11 notification program.
 *
 * Inspired by https://git.sr.ht/~crm/nfy/
 */

#include <sys/file.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define MAX_LNS 32
#define MAX_LN 80
#define PADDING 12
#define FONT "-*-terminus-medium-r-normal--22-*-*-*-c-*-iso10646-1"
#define FONT_COLOR 0xFFFFFF
#define BG_COLOR "#222222"
#define BORDER_COLOR "#005577"
#define BORDER_SZ 7
#define LINEGAP 8
#define LOCK_FILE "/tmp/.xnf.lock"

struct ln {
	char str[MAX_LN+1];
	int asc;
};

int
main(int argc, char** argv)
{
	Colormap colormap;
	XColor color;
	GC gc;
	XCharStruct xch;
	XEvent xev;
	XSetWindowAttributes wattrs = {0};
	Display* dpl;
	Screen* screen;
	Visual* vis;
	XFontStruct* font;
	XGCValues* gcv;
	struct ln lns[MAX_LNS];
	Window w;
	int dummy;
	int i, j;
	int lns_l;
	int lock_fd;
	int scr, scr_w, scr_h;
	int w_x, w_y, w_w, w_h;
	pid_t pid;
	char pesc;

	if (argc == 1 || argc > 3)
		errx(1,
"Usage: xnf message [tl | tc | tr | bl | bc | br | cl | cc | cr]");

	j = 0;
	lns_l = 0;
	pesc = 0;
	/*
	 * Split message string into lines.
	 */
	for (i = 0; argv[1][i] != '\0'; i++) {
		switch (argv[1][i]) {
		case 'n':
			/* Means ``\n''. */
			if (pesc == 1) {
				pesc = 0;
				lns[lns_l++].str[j] = '\0';
				j = 0;
				continue;
			}
			/* FALLTHROUGH. */
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
	if ((pid = fork()) == -1)
		err(1, "fork()");
	if (pid != 0)
		_exit(0);
	if (setsid() == -1)
		err(1, "setsid()");
	if ((lock_fd = open(LOCK_FILE, O_CREAT | O_WRONLY, 0600)) == -1)
		err(1, "open()");
	if (flock(lock_fd, LOCK_EX) == -1)
		err(1, "flock()");
	dpl = XOpenDisplay(NULL);
	if (dpl == NULL)
		err(1, "XOpenDisplay()");
	scr = DefaultScreen(dpl);
	screen = DefaultScreenOfDisplay(dpl);
	scr_w = WidthOfScreen(screen);
	scr_h = HeightOfScreen(screen);
	vis = DefaultVisual(dpl, scr);
	colormap = DefaultColormap(dpl, scr);
	font = XLoadQueryFont(dpl, FONT);
	wattrs.override_redirect = True;
	XAllocNamedColor(dpl, colormap, BG_COLOR, &color, &color);
	wattrs.background_pixel = color.pixel;
	XAllocNamedColor(dpl, colormap, BORDER_COLOR, &color, &color);
	wattrs.border_pixel = color.pixel;
	w_w = 0;
	w_h = PADDING * 2 + (lns_l-1) * LINEGAP;
	for (i = 0; i < lns_l; i++) {
		XTextExtents(font, lns[i].str, strlen(lns[i].str), &dummy,
		    &dummy, &dummy, &xch);
		if (xch.rbearing - xch.lbearing > w_w)
			w_w = xch.rbearing - xch.lbearing;
		lns[i].asc = xch.ascent;
		w_h += xch.ascent;
	}
	w_w += PADDING * 2;
	w_y = 0;
	w_x = scr_w - w_w - BORDER_SZ * 2;
	if (argc == 3) {
		switch (argv[2][0]) {
		case 't':
			w_y = 0;
			break;
		case 'b':
			w_y = scr_h - w_h - BORDER_SZ * 2;
			break;
		case 'c':
			w_y = scr_h / 2 - (w_h / 2) - BORDER_SZ;
			break;
		default:
			errx(1, "Wrong placement");
		}
		switch(argv[2][1]) {
		case 'l':
			w_x = 0;
			break;
		case 'r':
			w_x = scr_w - w_w - BORDER_SZ * 2;
			break;
		case 'c':
			w_x = scr_w / 2 - (w_w / 2) - BORDER_SZ;
			break;
		default:
			errx(1, "Wrong placement");
		}
	}
	w = XCreateWindow(dpl, DefaultRootWindow(dpl), w_x, w_y,
	    w_w, w_h, BORDER_SZ, DefaultDepth(dpl, scr), CopyFromParent,
	    vis, CWOverrideRedirect | CWBackPixel | CWBorderPixel, &wattrs);
	gcv = 0;
	gc = XCreateGC(dpl, w, 0, gcv);
	XSetFont(dpl, gc, font->fid);
	XSetForeground(dpl, gc, FONT_COLOR);
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
				XDrawString(dpl, w, gc, PADDING, PADDING +
				    LINEGAP * i + (j += lns[i].asc), lns[i].str,
				    strlen(lns[i].str));
			break;
		case ButtonPress:
			goto end;
		}
	}
end:
	XCloseDisplay(dpl);
	close(lock_fd);

	return 0;
}
