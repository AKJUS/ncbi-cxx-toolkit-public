# $Id$

APP = db100_numeric
SRC = numeric common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS100_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds100 tds_ftds100
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db100 --no-auto db100_numeric
CHECK_COPY = numeric.sql numeric_2.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
