#include "common.h"

#include <freetds/test_assert.h>

static char *
add_char(char *s, SQLWCHAR ch)
{
	if (ch == '\\')
		s += sprintf(s, "\\\\");
	else if (ch == '\t')
		s += sprintf(s, "\\t");
	else if (ch == '\r')
		s += sprintf(s, "\\r");
	else if (ch == '\n')
		s += sprintf(s, "\\n");
	else if ((unsigned int) ch < 32u)
		s += sprintf(s, "\\x%02x", (unsigned int) ch);
	else if ((unsigned int) ch < 256u)
		s += sprintf(s, "%c", (char) ch);
	else
		s += sprintf(s, "\\u%04x", (unsigned int) ch);
	return s;
}

void
odbc_c2string(char *out, SQLSMALLINT out_c_type, const void *in, size_t in_len)
{
	typedef union {
		unsigned char bin[256];
		char s[256];
		SQLWCHAR ws[256/sizeof(SQLWCHAR)];
		SQLINTEGER i;
		SQLBIGINT bi;
		SQLSMALLINT si;
		SQL_NUMERIC_STRUCT num;
		SQL_TIMESTAMP_STRUCT ts;
	} buf_t;
#undef IN
#define IN (*((const buf_t*) in))
        size_t i;
	const SQL_NUMERIC_STRUCT *num;
	char *s;

	out[0] = 0;
	s = out;
	switch (out_c_type) {
	case SQL_C_NUMERIC:
		num = &IN.num;
		s += sprintf(s, "%d %d %d ", num->precision, num->scale, num->sign);
		i = SQL_MAX_NUMERIC_LEN;
		for (; i > 0 && !num->val[--i];)
			continue;
                for (; (int) i >= 0; --i)
			s += sprintf(s, "%02X", num->val[i]);
		break;
	case SQL_C_BINARY:
		assert(in_len >= 0);
		for (i = 0; i < in_len; ++i)
			s += sprintf(s, "%02X", IN.bin[i]);
		break;
	case SQL_C_CHAR:
		assert(IN.s[in_len] == 0);
		s += sprintf(s, "%u ", (unsigned int) in_len);
		for (i = 0; i < in_len; ++i)
			s = add_char(s, (unsigned char) IN.s[i]);
		*s = 0;
		break;
	case SQL_C_WCHAR:
		assert(in_len >=0 && (in_len % sizeof(SQLWCHAR)) == 0);
		s += sprintf(s, "%u ", (unsigned int) (in_len / sizeof(SQLWCHAR)));
		for (i = 0; i < in_len / sizeof(SQLWCHAR); ++i)
			s = add_char(s, IN.ws[i]);
		*s = 0;
		break;
	case SQL_C_LONG:
		assert(in_len == sizeof(SQLINTEGER));
		sprintf(s, "%ld", (long int) IN.i);
		break;
	case SQL_C_SBIGINT:
		assert(in_len == sizeof(SQLBIGINT));
                sprintf(s, "%" PRId64, (int64_t) IN.bi);
		break;
	case SQL_C_SHORT:
		assert(in_len == sizeof(SQLSMALLINT));
		sprintf(s, "%d", (int) IN.si);
		break;
	case SQL_C_TIMESTAMP:
		sprintf(s, "%04d-%02u-%02u %02u:%02u:%02u.%03u",
			IN.ts.year, IN.ts.month, IN.ts.day,
			IN.ts.hour, IN.ts.minute, IN.ts.second,
			(unsigned) (IN.ts.fraction / 1000000u));
		break;
	default:
		/* not supported */
		assert(0);
		break;
	}
}
