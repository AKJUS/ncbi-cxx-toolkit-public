# $Id$

APP = unit_test_seq_trimmer
SRC = unit_test_seq_trimmer

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = $(OBJREAD_LIBS) xobjutil test_boost $(SOBJMGR_LIBS)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD  =
CHECK_COPY = test_data

WATCHERS = vasilche dicuccio stakhovv foleyjp
