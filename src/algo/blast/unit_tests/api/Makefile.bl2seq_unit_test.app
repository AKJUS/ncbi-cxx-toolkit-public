# $Id$

APP = bl2seq_unit_test
SRC = bl2seq_unit_test

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIB = blast_unit_test_util test_boost $(BLAST_INPUT_LIBS) \
    $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL)) 
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(NETWORK_LIBS) \
       $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)


CHECK_REQUIRES = MT
CHECK_CMD = bl2seq_unit_test
CHECK_COPY = bl2seq_unit_test.ini

WATCHERS = boratyng camacho fongah2
