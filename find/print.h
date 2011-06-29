#include "defs.h"

struct format_val;
struct parser_table;
struct predicate;
struct segment;

struct segment **make_segment (struct segment **segment,
			       char *format, int len,
			       int kind, char format_char,
			       char aux_format_char,
			       struct predicate *pred);
bool
insert_fprintf (struct format_val *vec,
		const struct parser_table *entry,
		char *format);
