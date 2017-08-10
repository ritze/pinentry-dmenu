/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 1;
static int minpwlen = 32;
static const char *fonts[] = {
	"Noto Sans UI:size=13" // "monospace:size=10"
};
static const char *prompt = "🔑"; //NULL;
static const char *asterisk = "● "; //"*";
static const char *colors[SchemeLast][2] = {
	[SchemeNorm] = { "#ffffff", "#000000" }, // "#bbbbbb", "#222222" },
	[SchemeSel] = { "#eeeeee", "#d9904a" } // "#eeeeee", "#005577" }
};
