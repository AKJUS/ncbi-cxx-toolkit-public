#
# Makefile:  /export/home/madden/64bit_build/ICCRelease64MT_BigPrime/trunk/internal/c++/src/internal/blast/proteinkmer/ckblastindex/Makefile.ckblastindex.app
#
# This file was originally generated by shell script "new_project" (r442644)
# Mon Aug  3 09:25:40 EDT 2015
#

###  BASIC PROJECT SETTINGS
APP = ckblastindex
SRC = ckblastindex
# OBJ =

LIB_ = proteinkmer $(BLAST_INPUT_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)

