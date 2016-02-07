#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

#include "pinentry/pinentry.h"
#include "pinentry/memory.h"

/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTNW(X,N)           (drw_font_getexts_width(drw->fonts[0], (X), (N)))
#define TEXTW(X)              (drw_text(drw, 0, 0, 0, 0, (X), 0) + drw->fonts[0]->h)

const char *str_OK  = "OK\n";
const char *str_ERRUNPARS = "ERR1337 dunno what to do with it\n";
const char *str_ERRNOTIMP = "ERR4100 not implemented yet\n";

/* enums */
enum { SchemeNorm, SchemeSel, SchemeLast }; /* color schemes */
enum {WinPin, WinConfirm}; /* window modes */
enum {Ok, NotOk, Cancel}; /* return status */

static char text[2048] = "";
static int bh, mw, mh;
static int inputw, promptw;
static size_t cursor = 0;
static Atom clip, utf8;
static Window win;
static XIC xic;
static int mon = -1;

static ClrScheme scheme[SchemeLast];
static Display *dpy;
static int screen;
static Window root;
static Drw *drw;
static int sw, sh;

static int timed_out;

static int confirmed;
static int winmode;

pinentry_t pinentry;

#include "config.h"

void
grabkeyboard(void) {
	int i;

	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for(i = 0; i < 1000; i++) {
		if(XGrabKeyboard(dpy, DefaultRootWindow(dpy), True,
		                 GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
			return;
		usleep(1000);
	}
	die("cannot grab keyboard\n");
}

size_t
nextrune(int cursor, int inc) {
	ssize_t n;

	/* return location of next utf8 rune in the given direction (+1 or -1) */
	for(n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc);
	return n;
}


void
insert(const char *str, ssize_t n) {
	if(strlen(text) + n > sizeof text - 1)
		return;
	/* move existing text out of the way, insert new text, and update cursor */
	memmove(&text[cursor + n], &text[cursor], sizeof text - cursor - MAX(n, 0));
	if(n > 0)
		memcpy(&text[cursor], str, n);
	cursor += n;
}

void
drawwin(void){
	int curpos;
	int x=0, y=0, h=bh, w;
	drw_setscheme(drw, &scheme[SchemeNorm]);
	drw_rect(drw, 0,0,mw,mh,True,1,1);

	if ((pinentry->description) && *(pinentry->description)) {
		drw_setscheme(drw, &scheme[SchemeSel]);
		drw_text(drw, 0,0,mw,bh,pinentry->description,0);
		y+=bh;
	}
	
	if ((pinentry->prompt) && *(pinentry->prompt)) {
		drw_setscheme(drw, &scheme[SchemeSel]);
		drw_text(drw, x, y, promptw, bh, pinentry->prompt, 0);
		x += promptw;
	}
	
	w = inputw;
	drw_setscheme(drw, &scheme[SchemeNorm]);
	
	char *sectext = malloc (sizeof (char) * 2048);
	sectext[0] = '\0';
	int i;
	int seccursor=0;
	for (i=0; text[i]!='\0'; i=nextrune(i, +1)){
		strcat(sectext, secchar);
		if (i<cursor){
			ssize_t n;
			for(n = seccursor + 1; n + 1 >= 0 && (sectext[n] & 0xc0) == 0x80; n ++);
			seccursor = n;
		}
	}
	if (winmode==WinPin) {
		drw_text(drw, x, y, mw, bh, sectext, 0);
		if((curpos = TEXTNW(sectext, seccursor) + bh/2 - 2) < w) {
			drw_rect(drw, x + curpos + 2, y + 2,  1 , bh-4 , True, 1, 0);
		}
	} else {
		drw_text(drw, x, y, mw, bh, "(y/n)", 0);
	}

	drw_map(drw, win, 0, 0, mw, mh);
}

void
setup(void){
	int x,y;
	XSetWindowAttributes swa;
	XIM xim;
	scheme[SchemeNorm].bg = drw_clr_create(drw, normbgcolor);
	scheme[SchemeNorm].fg = drw_clr_create(drw, normfgcolor);
	scheme[SchemeSel].bg = drw_clr_create(drw, selbgcolor);
	scheme[SchemeSel].fg = drw_clr_create(drw, selfgcolor);
	clip = XInternAtom(dpy, "CLIPBOARD",   False);
	utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	bh = drw->fonts[0]->h + 2;
	mh = (pinentry->description && *(pinentry->description))? bh * 2 : bh;
	x = 0;
	y = topbar ? 0 : sh - mh;
	mw = sw;
	promptw = (pinentry->prompt && *(pinentry->prompt)) ? TEXTW(pinentry->prompt) : 0;
	inputw = mw-promptw;
	swa.override_redirect = True;
	swa.background_pixel = scheme[SchemeNorm].bg->pix;
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
	win = XCreateWindow(dpy, root, x, y, mw, mh, 0,
	                    DefaultDepth(dpy, screen), CopyFromParent,
	                    DefaultVisual(dpy, screen),
	                    CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);

	xim = XOpenIM(dpy, NULL, NULL, NULL);
	xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	                XNClientWindow, win, XNFocusWindow, win, NULL);

	XMapRaised(dpy, win);
	drw_resize(drw, mw, mh);

	drawwin();
}

void
cleanup(void) {
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	drw_clr_free(scheme[SchemeNorm].bg);
	drw_clr_free(scheme[SchemeNorm].fg);
	drw_clr_free(scheme[SchemeSel].fg);
	drw_clr_free(scheme[SchemeSel].bg);
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
	if (status == XBufferOverflow)
		return 0;
	if (winmode == WinConfirm){
		switch(ksym){
		case XK_KP_Enter:
		case XK_Return:
		case XK_y:
			confirmed = 1;
			return 1;
			break;
		case XK_n:
			confirmed = 0;
			return 1;
			break;
		case XK_Escape:
			pinentry->canceled = 1;
			confirmed = 0;
			return 1;
			break;
		}
	} else {
		switch(ksym){
		default:
			if (!iscntrl(*buf))
				insert(buf, len);
			break;
		case XK_Delete:
			if(text[cursor] == '\0')
				return 0;
			cursor = nextrune(cursor, +1);
			/* fallthrough */
		case XK_BackSpace:
			if(cursor == 0)
				return 0;
			insert(NULL, nextrune(cursor, -1) - cursor);
			break;
		case XK_Escape:
			pinentry->canceled = 1;
			return 1;
			/*cleanup();
			exit(1);*/
			break;
		case XK_Left:
			if(cursor > 0) {
				cursor = nextrune(cursor, -1);
			}
			break;
		case XK_Right:
			if(text[cursor]!='\0') {
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

void
paste(void) {
	char *p, *q;
	int di;
	unsigned long dl;
	Atom da;

	/* we have been given the current selection, now insert it into input */
	XGetWindowProperty(dpy, win, utf8, 0, (sizeof text / 4) + 1, False,
	                   utf8, &da, &di, &dl, &dl, (unsigned char **)&p);
	insert(p, (q = strchr(p, '\n')) ? q-p : (ssize_t)strlen(p));
	XFree(p);
	drawwin();
}

void
run(void) {
	XEvent ev;
	while(!XNextEvent(dpy, &ev)) {
		if(XFilterEvent(&ev, win))
			continue; /*what is this I don't even*/
		switch(ev.type) {
		case Expose:
			if(ev.xexpose.count == 0)
				drw_map(drw, win, 0, 0, mw, mh);
			break;
		case KeyPress:
			if (keypress(&ev.xkey)) return;
			break;
		case SelectionNotify:
			if(ev.xselection.property == utf8)
				paste();
			break;
		case VisibilityNotify:
			if(ev.xvisibility.state != VisibilityUnobscured)
				XRaiseWindow(dpy, win);
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
catchsig(int sig)
{
	if (sig == SIGALRM)
		timed_out = 1;
}

static int
password (void) {
	winmode = WinPin;
	promptwin();
	if (pinentry->canceled) return -1;
	char *buf = secmem_malloc(strlen(text));
	strcpy(buf, text);
	pinentry_setbuffer_use (pinentry, buf, 0);
	return 1;
}

static int
confirm(void) {
	winmode = WinConfirm;
	confirmed = 0;
	promptwin();
	return confirmed;
}

static int
spinecmdhandler (pinentry_t recieved_pinentry) {
	text[0]='\0';
	cursor = 0;
	pinentry = recieved_pinentry;

	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if(!(dpy = XOpenDisplay(pinentry->display))) /*NULL was here*/
		die("dmenu: cannot open display\n");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	drw_load_fonts(drw, fonts, LENGTH(fonts));
	if(!drw->fontcount)
		die("No fonts could be loaded.\n");
	drw_setscheme(drw, &scheme[SchemeNorm]);

	if (pinentry->timeout){
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = catchsig;
		sigaction(SIGALRM, &sa, NULL);
		alarm(pinentry->timeout);
	}
	if (pinentry->pin)
		return password();
	else
		return confirm();
	return -1;
}

pinentry_cmd_handler_t pinentry_cmd_handler = spinecmdhandler;

int
main(int argc, char *argv[]) {
	pinentry_init("spine");
	pinentry_parse_opts(argc, argv);
	if (pinentry_loop())
		return 1;
	return 0;
}
