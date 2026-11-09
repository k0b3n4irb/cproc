#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "cc.h"

enum attrprefix {
	PREFIXNONE = 1,  /* standard attribute */
	PREFIXGNU,
};

/*
GNU attributes this compiler does not implement and whose omission would
change what the program means or where it is placed: skipping them, as is
done for every other unknown attribute, would compile a different program
without a word. They are refused, naming the attribute.
*/
static const struct {
	char *name, *why;
} refused[] = {
	{"alias", "the declaration would not name the other object"},
	{"cleanup", "the cleanup function would never be called"},
	{"ext_vector_type", "it makes a vector type"},
	{"mode", "it sets the width of the type"},
	{"ms_struct", "it changes the layout of the structure"},
	{"noinit", "it changes how the object is placed and initialized"},
	{"persistent", "it changes how the object is placed and initialized"},
	{"scalar_storage_order", "it changes the byte order of the members"},
	{"section", "it changes where the object is placed"},
	{"vector_size", "it makes a vector type"},
};

static char *
strip(char *name)
{
	size_t len;

	len = strlen(name);
	if (len >= 4 && name[0] == '_' && name[1] == '_' && name[len - 2] == '_' && name[len - 1] == '_') {
		name[len - 2] = '\0';
		name += 2;
	}
	return name;
}

static bool
parseattr(struct attr *a, enum attrkind allowed, enum attrprefix prefix)
{
	char *name, *prefixname = "";
	enum attrkind kind;
	int paren;
	size_t i;

	if (tok.kind != TIDENT)
		return false;
	name = strip(tok.lit);
	next();
	if (!prefix) {
		if (consume(TCOLONCOLON)) {
			if (strcmp(name, "gnu") == 0)
				prefix = PREFIXGNU;
			else
				prefix = 0;
			name = strip(expect(TIDENT, "after attribute prefix"));
		} else {
			prefix = PREFIXNONE;
		}
	}
	kind = 0;
	switch (prefix) {
	default: break;
	case PREFIXGNU:
		prefixname = "GNU ";
		if (strcmp(name, "aligned") == 0) {
			kind = ATTRALIGNED;
			if (consume(TLPAREN)) {
				unsigned long long i;

				i = intconstexpr(&filescope, false);
				if (!i || i & (i - 1) || i > INT_MAX)
					error(&tok.loc, "invalid alignment %llu", i);
				if (a)
					a->align = i;
				expect(TRPAREN, "after alignment");
			} else {
				if (a)
					a->align = 16;
			}
		} else if (strcmp(name, "constructor") == 0) {
			kind = ATTRCONSTRUCTOR;
		} else if (strcmp(name, "destructor") == 0) {
			kind = ATTRDESTRUCTOR;
		} else if (strcmp(name, "packed") == 0) {
			kind = ATTRPACKED;
		} else {
			for (i = 0; i < LEN(refused); ++i) {
				if (strcmp(name, refused[i].name) == 0)
					error(&tok.loc, "GNU attribute '%s' is not supported: %s", name, refused[i].why);
			}
		}
		break;
	}
	if (kind) {
		if (!(kind & allowed))
			error(&tok.loc, "%sattribute '%s' is not supported here", prefixname, name);
		if (a)
			a->kind |= kind;
	} else if (consume(TLPAREN)) {
		/* skip arguments */
		for (paren = 1; paren > 0; next()) {
			switch (tok.kind) {
			case TLPAREN: ++paren; break;
			case TRPAREN: --paren; break;
			default: break;
			}
		}
	}
	return true;
}

static bool
attrspec(struct attr *a, enum attrkind allowed)
{
	if (tok.kind != TLBRACK || !peek(TLBRACK))
		return false;
	while (parseattr(a, allowed, 0) || consume(TCOMMA))
		;
	expect(TRBRACK, "to end attribute specifier");
	expect(TRBRACK, "to end attribute specifier");
	return true;
}

bool
attr(struct attr *a, enum attrkind allowed)
{
	if (!attrspec(a, allowed))
		return false;
	while (attrspec(a, allowed))
		;
	return true;
}

static bool
gnuattrspec(struct attr *a, enum attrkind allowed)
{
	if (!consume(T__ATTRIBUTE__))
		return false;
	while (parseattr(a, allowed, PREFIXGNU) || consume(TCOMMA))
		;
	expect(TLPAREN, "after '__attribute__' to begin attribute specifier");
	expect(TLPAREN, "after '__attribute__' to begin attribute specifier");
	while (parseattr(a, allowed, PREFIXGNU) || consume(TCOMMA))
		;
	expect(TRPAREN, "to end attribute specifier");
	expect(TRPAREN, "to end attribute specifier");
	return true;
}

bool
gnuattr(struct attr *a, enum attrkind allowed)
{
	if (!gnuattrspec(a, allowed))
		return false;
	while (gnuattrspec(a, allowed))
		;
	return true;
}
