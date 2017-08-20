/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <libconfig.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

#include "pinentry/pinentry.h"
#include "pinentry/memory.h"

/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                            && MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)
#define MINDESCLEN 8

#define CONFIGPATH $HOME/.gnupg/pinentry-dmenu.conf

/* enums */
enum { SchemePrompt, SchemeNormal, SchemeSelect, SchemeDesc, SchemeLast }; /* color schemes */
enum { WinPin, WinConfirm }; /* window modes */
enum { Ok, NotOk, Cancel }; /* return status */
enum { Nothing, Yes, No }; /* confirm dialog */

static char text[BUFSIZ] = "";
//static char *text;
static char *embed;
static int bh, mw, mh;
static int sel;
static int promptw, ppromptw, pdescw;
static int lrpad; /* sum of left and right padding */
static size_t cursor;
static int mon = -1, screen;

static Atom clip, utf8;
static Display *dpy;
static Window root, parentwin, win;
static XIC xic;

static Drw *drw;
static Clr *scheme[SchemeLast];

static int timed_out;
static int winmode;
pinentry_t pinentry;

#include "config.h"

static int
drawitem(const char* text, Bool sel, int x, int y, int w) {
	unsigned int i = (sel) ? SchemeSelect : SchemeNormal;

	drw_setscheme(drw, scheme[i]);

	return drw_text(drw, x, y, w, bh, lrpad / 2, text, 0);
}

// XXX: Delete this function
static void
grabfocus(void) {
	Window focuswin;
	int i, revertwin;

	for (i = 0; i < 100; ++i) {
		XGetInputFocus(dpy, &focuswin, &revertwin);
		if (focuswin == win) {
			return;
		}
		XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
		usleep(1000);
	}

	die("cannot grab focus");
}

static void
grabkeyboard(void) {
	int i;

	if (embed) {
		return;
	}
	/* try to grab keyboard,
	 * we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync,
		                  GrabModeAsync, CurrentTime) == GrabSuccess) {
			return;
		}
		usleep(1000);
	}

	die("cannot grab keyboard");
}

static size_t
nextrune(int cursor, int inc) {
	ssize_t n;

	/* return location of next utf8 rune in the given direction (+1 or -1) */
	for (n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc);
	
	return n;
}

static void
insert(const char *str, ssize_t n) {
	if (strlen(text) + n > sizeof text - 1) {
		return;
	}
	
	/* move existing text out of the way, insert new text, and update cursor */
	memmove(&text[cursor + n], &text[cursor], sizeof text - cursor - MAX(n, 0));

	if (n > 0) {
		memcpy(&text[cursor], str, n);
	}
	cursor += n;
}

static void
drawwin(void) {
	unsigned int curpos;
	int x = 0, pb = 0, pbw = 0, i;
	size_t asterlen = strlen(asterisk);
	size_t pdesclen;
	char* desc;
	char* censort = ecalloc(1, asterlen * sizeof(text));
	
	unsigned int censortl = minpwlen * TEXTW(asterisk) / strlen(asterisk);
	unsigned int confirml = TEXTW(" YesNo ") + 3 * lrpad;

	drw_setscheme(drw, scheme[SchemeNormal]);
	drw_rect(drw, 0, 0, mw, mh, 1, 1);

	if (prompt && *prompt) {
		drw_setscheme(drw, scheme[SchemePrompt]);
		x = drw_text(drw, x, 0, promptw, bh, lrpad / 2, prompt, 0);
	}

	if (pinentry->prompt && *pinentry->prompt) {
		drw_setscheme(drw, scheme[SchemePrompt]);
		drw_text(drw, x, 0, ppromptw, bh, lrpad / 2, pinentry->prompt, 0);
		x += ppromptw;
	}

	if (pinentry->description && *pinentry->description) {
		pb = mw - x;
		pdesclen = strlen(pinentry->description);
		
		if (pb > 0) {
			pb -= (winmode == WinPin) ? censortl : confirml;
			pbw = MINDESCLEN * pdescw / pdesclen;
			pbw = MIN(pbw, pdescw);

			if (pb >= pbw) {
				pbw = MAX(pbw, pdescw);
				pbw = MIN(pbw, pb);
				pb = mw - pbw;
				desc = pinentry->description;
				drw_setscheme(drw, scheme[SchemeDesc]);
				drw_text(drw, pb, 0, pbw, bh, lrpad / 2, desc, 0);
			} else {
				pb = 0;
			}
		} else {
			pb = 0;
		}
	}

	/* Draw input field */
	drw_setscheme(drw, scheme[SchemeNormal]);

	if (winmode == WinPin) {
		for (i = 0; i < asterlen * strlen(text); i += asterlen) {
			memcpy(&censort[i], asterisk, asterlen);
		}

		censort[i+1] = '\n';
		drw_text(drw, x, 0, mw - x - pbw, bh, lrpad / 2, censort, 0);
		drw_font_getexts(drw->fonts, censort, cursor * asterlen, &curpos, NULL);
		free(censort);
	} else {
		x += TEXTW(" ");
		x = drawitem("No", (sel == No), x, 0, TEXTW("No"));
		x = drawitem("Yes", (sel == Yes), x, 0, TEXTW("Yes"));
	}

	drw_map(drw, win, 0, 0, mw, mh);
}

static void
setup(void) {
	int x, y, i = 0;
	unsigned int du;
	const char* pprompt = pinentry->prompt;
	const char* pdesc = pinentry->description;
	XSetWindowAttributes swa;
	XIM xim;
	Window w, dw, *dws;
	XWindowAttributes wa;
#ifdef XINERAMA
	XineramaScreenInfo *info;
	Window pw;
	int a, j, di, n, area = 0;
#endif

	/* Init appearance */
	scheme[SchemePrompt] = drw_scm_create(drw, colors[SchemePrompt], 2);
	scheme[SchemeNormal] = drw_scm_create(drw, colors[SchemeNormal], 2);
	scheme[SchemeSelect] = drw_scm_create(drw, colors[SchemeSelect], 2);
	scheme[SchemeDesc]   = drw_scm_create(drw, colors[SchemeDesc],   2);

	text[0] = '\0';
	clip = XInternAtom(dpy, "CLIPBOARD",   False);
	utf8 = XInternAtom(dpy, "UTF8_STRING", False);

	/* Calculate menu geometry */
	bh = drw->fonts->h + 2;
	mh = bh;
#ifdef XINERAMA
	info = XineramaQueryScreens(dpy, &n);

	if (parentwin == root && info) {
		XGetInputFocus(dpy, &w, &di);
		if (mon >= 0 && mon < n) {
			i = mon;
		} else if (w != root && w != PointerRoot && w != None) {
			/* find top-level window containing current input focus */
			do {
				if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws) {
					XFree(dws);
				}
			} while (w != root && w != pw);
			/* find xinerama screen with which the window intersects most */
			if (XGetWindowAttributes(dpy, pw, &wa)) {
				for (j = 0; j < n; j++) {
					a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j]);
					if (a > area) { 
						area = a;
						i = j;
					}
				}
			}
		}
		/* no focused window is on screen, so use pointer location instead */
		if (mon < 0 && !area
		    && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du)) {
			for (i = 0; i < n; i++) {
				if (INTERSECT(x, y, 1, 1, info[i])) {
					break;
				}
			}
		}

		x = info[i].x_org;
		y = info[i].y_org + (topbar ? 0 : info[i].height - mh);
		mw = info[i].width;
		XFree(info);
	} else
#endif
	{
		if (!XGetWindowAttributes(dpy, parentwin, &wa)) {
			die("could not get embedding window attributes: 0x%lx", parentwin);
		}
		x = 0;
		y = topbar ? 0 : wa.height - mh;
		mw = wa.width;
	}

	promptw = (prompt && *prompt) ? TEXTW(prompt) - lrpad / 4 : 0;
	ppromptw = (pprompt && *pprompt) ? TEXTW(pprompt) : 0;
	pdescw = (pdesc && *pdesc) ? TEXTW(pdesc) : 0;

	/* create menu window */
	swa.override_redirect = True;
	swa.background_pixel = scheme[SchemePrompt][ColBg].pixel;
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
	win = XCreateWindow(dpy, parentwin, x, y, mw, mh, 0,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);

	/* Open input methods */
	xim = XOpenIM(dpy, NULL, NULL, NULL);
	xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	                XNClientWindow, win, XNFocusWindow, win, NULL);
	XMapRaised(dpy, win);

	if (embed) {
		XSelectInput(dpy, parentwin, FocusChangeMask);

		if (XQueryTree(dpy, parentwin, &dw, &w, &dws, &du) && dws) {
			for (i = 0; i < du && dws[i] != win; ++i) {
				XSelectInput(dpy, dws[i], FocusChangeMask);
			}

			XFree(dws);
		}
		grabfocus();
	}

	drw_resize(drw, mw, mh);
	drawwin();
}

static void
cleanup(void) {
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	free(scheme[SchemeDesc]);
	free(scheme[SchemeSelect]);
	free(scheme[SchemeNormal]);
	free(scheme[SchemePrompt]);
	drw_free(drw);
	XSync(dpy, False);
	XCloseDisplay(dpy);
}

static int
keypress(XKeyEvent *ev) {
	char buf[32];
	int len;

	KeySym ksym = NoSymbol;
	Status status;
	len = XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
	
	if (status == XBufferOverflow) {
		return 0;
	}

	if (winmode == WinConfirm) {
		switch(ksym) {
		case XK_KP_Enter:
		case XK_Return:
			if (sel != Nothing) {
				return 1;
			}
			break;
		case XK_y:
		case XK_Y:
			sel = Yes;
			return 1;
		case XK_n:
		case XK_N:
			sel = No;
			return 1;
		case XK_Escape:
			pinentry->canceled = 1;
			sel = No;
			return 1;
		case XK_Left:
			sel = No;
			break;
		case XK_Right:
			sel = Yes;
			break;
		}
	} else {
		switch(ksym) {
		default:
			if (!iscntrl(*buf)) {
				insert(buf, len);
			}
			break;
		case XK_Delete:
			if (text[cursor] == '\0') {
				return 0;
			}
			cursor = nextrune(cursor, +1);
			/* fallthrough */
		case XK_BackSpace:
			if (cursor == 0) {
				return 0;
			}
			insert(NULL, nextrune(cursor, -1) - cursor);
			break;
		case XK_Escape:
			pinentry->canceled = 1;
			return 1;
			/*cleanup();
			exit(1);*/
			break;
		case XK_Left:
			if (cursor > 0) {
				cursor = nextrune(cursor, -1);
			}
			break;
		case XK_Right:
			if (text[cursor] != '\0') {
				cursor = nextrune(cursor, +1);
			}
			break;
		case XK_Return:
		case XK_KP_Enter:
			return 1;
			break;
		}
	}

	drawwin();

	return 0;
}

static void
paste(void) {
	char *p, *q;
	int di;
	unsigned long dl;
	Atom da;

	/* We have been given the current selection, now insert it into input */
	XGetWindowProperty(dpy, win, utf8, 0, (sizeof text / 4) + 1, False,
	                   utf8, &da, &di, &dl, &dl, (unsigned char **)&p);
	insert(p, (q = strchr(p, '\n')) ? q - p : (ssize_t) strlen(p));
	XFree(p);
	drawwin();
}

void
run(void) {
	XEvent ev;
	while(!XNextEvent(dpy, &ev)) {
		if (XFilterEvent(&ev, win)) {
			continue; /* what is this I don't even */
		}
		switch(ev.type) {
		case Expose:
			if (ev.xexpose.count == 0) {
				drw_map(drw, win, 0, 0, mw, mh);
			}
			break;
		case KeyPress:
			if (keypress(&ev.xkey)) {
				return;
			}
			break;
		case SelectionNotify:
			if (ev.xselection.property == utf8) {
				paste();
			}
			break;
		case VisibilityNotify:
			if (ev.xvisibility.state != VisibilityUnobscured) {
				XRaiseWindow(dpy, win);
			}
			break;
		}
	}
}

void
promptwin(void) {
	grabkeyboard();
	setup();
	run();
	cleanup();
}

static void
catchsig(int sig) {
	if (sig == SIGALRM) {
		timed_out = 1;
	}
}

static int
password(void) {
	char *buf;
//	text = secmem_malloc(BUFSIZ);

	winmode = WinPin;
	promptwin();
	
	if (pinentry->canceled) {
//		secmem_free(text);
		return -1;
	}
//printf("text = %s\n", text);
buf = secmem_malloc(strlen(text));
strcpy(buf, text);

//	pinentry_setbuffer_use(pinentry, text, 0);
pinentry_setbuffer_use(pinentry, buf, 0);
//secmem_free(buf);
//	secmem_free(text);
	
	return 1;
}

static int
confirm(void) {
	winmode = WinConfirm;
	sel = Nothing;
	promptwin();
	
	return sel != No;
}

static int
cmdhandler(pinentry_t received_pinentry) {
	struct sigaction sa;
	XWindowAttributes wa;
	
	cursor = 0;
	pinentry = received_pinentry;

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
		fputs("warning: no locale support\n", stderr);
	}
	if (!(dpy = XOpenDisplay(pinentry->display))) {
		die("cannot open display");
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	if (!embed || !(parentwin = strtol(embed, NULL, 0))) {
		parentwin = root;
	}
	if (!XGetWindowAttributes(dpy, parentwin, &wa)) {
		die("could not get embedding window attributes: 0x%lx", parentwin);
	}
	drw = drw_create(dpy, screen, root, wa.width, wa.height);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts))) {
		die("no fonts could be loaded.");
	}
	lrpad = drw->fonts->h;
	drw_setscheme(drw, scheme[SchemePrompt]);

	if (pinentry->timeout) {
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = catchsig;
		sigaction(SIGALRM, &sa, NULL);
		alarm(pinentry->timeout);
	}

	if (pinentry->pin) {
		return password();
	} else {
		return confirm();
	}
	
	return -1;
}

pinentry_cmd_handler_t pinentry_cmd_handler = cmdhandler;

int
main(int argc, char *argv[]) {
	Bool bval;
	int val;
	const char *str;

	config_t cfg;
	config_setting_t *setting;

	config_init(&cfg);

	/* Read the file. If there is an error, report it and exit. */
	if (!config_read_file(&cfg, "example.cfg")) {
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
		        config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return(EXIT_FAILURE);
	}

	if (config_lookup_string(&cfg, "asterisk", &str)) {
		asterisk = str;
	}
	if (config_lookup_bool(&cfg, "buttom", &bval)) {
		topbar = !bval;
	}
	if (config_lookup_int(&cfg, "min_password_length", &val)) {
		minpwlen = val;
	}
	if (config_lookup_int(&cfg, "monitor", &val)) {
		mon = val;
	}
	if (config_lookup_string(&cfg, "prompt", &str)) {
		prompt = str;
	}

	if (config_lookup_string(&cfg, "font", &str)) {
		fonts[0] = str;
	}
	if (config_lookup_string(&cfg, "prompt_bg", &str)) {
		colors[SchemePrompt][ColBg] = str;
	}
	if (config_lookup_string(&cfg, "prompt_fg", &str)) {
		colors[SchemePrompt][ColFg] = str;
	}
	if (config_lookup_string(&cfg, "normal_bg", &str)) {
		colors[SchemeNormal][ColBg] = str;
	}
	if (config_lookup_string(&cfg, "normal_fg", &str)) {
		colors[SchemeNormal][ColFg] = str;
	}
	if (config_lookup_string(&cfg, "select_bg", &str)) {
		colors[SchemeSelect][ColBg] = str;
	}
	if (config_lookup_string(&cfg, "select_fg", &str)) {
		colors[SchemeSelect][ColFg] = str;
	}
	if (config_lookup_string(&cfg, "desc_bg", &str)) {
		colors[SchemeDesc][ColBg] = str;
	}
	if (config_lookup_string(&cfg, "desc_fg", &str)) {
		colors[SchemeDesc][ColFg] = str;
	}
	if (config_lookup_string(&cfg, "window_id", &str)) {
		embed = str;
	}
	
	pinentry_init("pinentry-dmenu");
	pinentry_parse_opts(argc, argv);
	
	if (pinentry_loop()) {
		return 1;
	}

	config_destroy(&cfg);
	
	return 0;
}
