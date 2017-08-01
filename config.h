/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 1;
static const char *fonts[] = {
	"monospace:size=10"
};
static const char *prompt = NULL;
static const char *asterisk = "*";
static const char *colors[SchemeLast][2] = {
	[SchemeNorm] = { "#bbbbbb", "#222222" },
	[SchemeSel] = { "#eeeeee", "#005577" }
};
