############################################################################
#
# Note:
# This file is included before everything else
# Anything related to the initial state should go early in this file!

if (WIN32)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(CMAKE_BUILD_TYPE Debug)
    endif()
    if ("${BUILD_SHARED_LIBS}" STREQUAL "")
        set(BUILD_SHARED_LIBS OFF)
    endif()
endif()

cmake_policy(SET CMP0054 OLD)

############################################################################
#
# Hunter packages for Windows
#

if (WIN32)
    #set(HUNTER_STATUS_DEBUG TRUE)
    hunter_add_package(wxWidgets)
    hunter_add_package(Boost COMPONENTS filesystem regex system)
    hunter_add_package(ZLIB)
    hunter_add_package(BZip2)
    hunter_add_package(Jpeg)
    hunter_add_package(PNG)
    hunter_add_package(TIFF)
    #hunter_add_package(freetype)
endif()

############################################################################
#
# Basic Setup
#


set(top_src_dir     ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(abs_top_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(build_root      ${CMAKE_BINARY_DIR})
set(builddir        ${CMAKE_BINARY_DIR})
set(includedir0     ${top_src_dir}/include)
set(includedir      ${includedir0})
set(incdir          ${build_root}/inc)
set(incinternal     ${includedir0}/internal)

# NOTE:
# We conditionally set a package config path
# The existence of files in this directory switches find_package()
# to automatically switch between module mode vs. config mode
#
set(NCBI_TOOLS_ROOT $ENV{NCBI})
if (EXISTS ${NCBI_TOOLS_ROOT})
    set(_NCBI_DEFAULT_PACKAGE_SEARCH_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/ncbi-defaults")
    set(CMAKE_PREFIX_PATH
        ${CMAKE_PREFIX_PATH}
        ${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}
        )
    message(STATUS "NCBI Root: ${NCBI_TOOLS_ROOT}")
    message(STATUS "CMake Prefix Path: ${CMAKE_PREFIX_PATH}")
else()
    message(STATUS "NCBI Root: <not found>")
endif()

if(WIN32)
    # Specific location overrides for Windows packages
    # Note: this variable must be set in the CMake GUI!!
    #set(WIN32_PACKAGE_ROOT "C:/Users/dicuccio/dev/packages")
    set(GIF_ROOT "${WIN32_PACKAGE_ROOT}/giflib-4.1.4-1-lib")
    set (ENV{GIF_DIR} "${GIF_ROOT}")

    set(PCRE_PKG_ROOT "${WIN32_PACKAGE_ROOT}/pcre-7.0-lib")
    set(PCRE_PKG_INCLUDE_DIRS "${PCRE_PKG_ROOT}/include")
    set(PCRE_PKG_LIBRARY_DIRS "${PCRE_PKG_ROOT}/lib")

    set(GNUTLS_ROOT "${WIN32_PACKAGE_ROOT}/gnutls-3.4.9")
    set(PC_GNUTLS_INCLUDEDIR "${GNUTLS_ROOT}/include")
    set(PC_GNUTLS_LIBDIR "${GNUTLS_ROOT}/lib")

    set(FREETYPE_ROOT "${WIN32_PACKAGE_ROOT}/freetype-2.3.5-1-lib")
    set(ENV{FREETYPE_DIR} "${FREETYPE_ROOT}")

    set(FTGL_ROOT "${WIN32_PACKAGE_ROOT}/ftgl-2.1.3_rc5")
    set(LZO_ROOT "${WIN32_PACKAGE_ROOT}/lzo-1.08-lib")

    set(CMAKE_PREFIX_PATH
        ${CMAKE_PREFIX_PATH}
        "${PCRE_PKG_ROOT}"
        "${GNUTLS_ROOT}"
        "${LZO_ROOT}"
        "${GIF_ROOT}"
        "${WIN32_PACKAGE_ROOT}/libxml2-2.7.8.win32"
        "${WIN32_PACKAGE_ROOT}/libxslt-1.1.26.win32"
        "${WIN32_PACKAGE_ROOT}/iconv-1.9.2.win32"
        "${FREETYPE_ROOT}"
        "${WIN32_PACKAGE_ROOT}/glew-1.5.8"
        "${FTGL_ROOT}"
        )
endif()

############################################################################
#
# Build revision stuff
# This is needed for some use cases
if (EXISTS ${top_src_dir}/.svn)
    include(FindSubversion)
    Subversion_WC_INFO(${top_src_dir} TOOLKIT)
    Subversion_WC_INFO(${top_src_dir}/src/corelib CORELIB)
else()
    set(TOOLKIT_WC_REVISION 0)
    set(TOOLKIT_WC_URL "")
    set(CORELIB_WC_REVISION 0)
    set(CORELIB_WC_URL "")
endif()

set(NCBI_SUBVERSION_REVISION ${TOOLKIT_WC_REVISION})
message(STATUS "SVN revision = ${TOOLKIT_WC_REVISION}")
message(STATUS "SVN URL = ${TOOLKIT_WC_URL}")

if ("$ENV{NCBI_TEAMCITY_BUILD_NUMBER}" STREQUAL "")
    set(NCBI_TEAMCITY_BUILD_NUMBER 0)
    message(STATUS "TeamCity build number = ${NCBI_TEAMCITY_BUILD_NUMBER}")
endif()

set(NCBI_SC_VERSION 0)
if (NOT "${CORELIB_WC_URL}" STREQUAL "")
    string(REGEX REPLACE ".*/([0-9]+)\\.[0-9]+/.*" "\\1" "${CORELIB_WC_URL}" _SC_VER)
    string(LENGTH "${_SC_VER}" _SC_VER_LEN)
    if (${_SC_VER_LEN} LESS 10 AND NOT "${_SC_VER}" STREQUAL "")
        set(NCBI_SC_VERSION ${_SC_VER})
        message(STATUS "Stable Components Number = ${NCBI_SC_VERSION}")
    endif()
endif()

#
# Initial messaging about some important stuff
#
message(STATUS "CMake Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build shared libraries: ${BUILD_SHARED_LIBS}")

# pass these back for ccache to pick up
set(ENV{CCACHE_UMASK} 002)
set(ENV{CCACHE_BASEDIR} ${top_src_dir})

if (NOT buildconf)
  set(buildconf "${CMAKE_BUILD_TYPE}MT64")
  set(buildconf0 ${CMAKE_BUILD_TYPE})
endif (NOT buildconf)

get_filename_component(top_src_dir "${top_src_dir}" REALPATH)
get_filename_component(abs_top_src_dir "${abs_top_src_dir}" REALPATH)
get_filename_component(build_root "${build_root}" REALPATH)
get_filename_component(includedir "${includedir}" REALPATH)

get_filename_component(EXECUTABLE_OUTPUT_PATH "${build_root}/../bin" REALPATH)
get_filename_component(LIBRARY_OUTPUT_PATH "${build_root}/../lib" REALPATH)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/" ${CMAKE_MODULE_PATH})

# Establishing compiler definitions
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.compiler.cmake)

# Establishing sane RPATH definitions
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, use the install RPATH already
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

SET(CMAKE_INSTALL_RPATH "/$ORIGIN/../lib")

#this add RUNPATH to binaries (RPATH is already there anyway), which makes it more like binaries built by C++ Toolkit
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH true)


############################################################################
#
# Testing
enable_testing()


#
# Basic checks
#
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.basic-checks.cmake)

#
# Framework for dealing with external libraries
#
include(${top_src_dir}/src/build-system/cmake/FindExternalLibrary.cmake)

############################################################################
#
# PCRE additions
#
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.pcre.cmake)

############################################################################
#
# Compression libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.compress.cmake)



############################################################################
#
# OS-specific settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.os.cmake)


#################################
# Some platform-specific system libs that can be linked eventually
set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
if (WIN32)
    set(KSTAT_LIBS    "")
    set(RPCSVC_LIBS   "")
    set(DEMANGLE_LIBS "")
    set(ICONV_LIBS    "")
    set(UUID_LIBS      "")
    set(NETWORK_LIBS  "ws2_32.lib")
    set(RT_LIBS          "")
    set(MATH_LIBS      "")
    set(CURL_LIBS      "")
    set(BERKELEYDB_INCLUDE      "")
    set(MYSQL_INCLUDE_DIR      "")
    set(SQLITE3_INCLUDE_DIR "")
    set(SQLITE3_LIBRARY "")
endif()

find_library(UUID_LIBS NAMES uuid)
find_library(CRYPT_LIBS NAMES crypt)
find_library(MATH_LIBS NAMES m)

if (APPLE)
  find_library(NETWORK_LIBS c)
  find_library(RT_LIBS c)
else (APPLE)
  find_library(NETWORK_LIBS nsl)
  find_library(RT_LIBS        rt)
endif (APPLE)

#
# Basic Library Definitions
# FIXME: get rid of these
#

set(ORIG_LIBS   ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set(ORIG_C_LIBS            ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set(C_LIBS      ${ORIG_C_LIBS})


# ############################################################
# Specialized libs settings
# Mostly, from Makefile.mk
# ############################################################
#

set(LIBS ${LIBS} ${DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


#
# NCBI-specific library subsets
# NOTE:
# these should be eliminated or simplified; they're not needed
#
set(LOCAL_LBSM ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd)

############################################################################
#
# OS-specific settings
if (UNIX)
    find_library(GMP_LIB LIBS gmp HINTS "$ENV{NCBI}/gmp-6.0.0a/lib64/")
    find_library(IDN_LIB LIBS idn HINTS "/lib64")
    find_library(NETTLE_LIB LIBS nettle HINTS "$ENV{NCBI}/nettle-3.1.1/lib64")
    find_library(HOGWEED_LIB LIBS hogweed HINTS "$ENV{NCBI}/nettle-3.1.1/lib64")
elseif (WIN32)
    set(GMP_LIB "")
    set(IDN_LIB "")
    set(NETTLE_LIB "")
    set(HOGWEED_LIB "")
endif()

find_package(GnuTLS)
if (GnuTLS_FOUND)
    set(GNUTLS_LIBRARIES ${GNUTLS_LIBRARIES} ${ZLIB_LIBRARIES} ${IDN_LIB} ${RT_LIBS} ${HOGWEED_LIB} ${NETTLE_LIB} ${GMP_LIB})
    set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})
endif()

############################################################################
#
# Kerberos 5 (via GSSAPI)
# FIXME: replace with native CMake check
#
find_external_library(KRB5 INCLUDES gssapi/gssapi_krb5.h LIBS gssapi_krb5 krb5 k5crypto com_err)


############################################################################
#
# Sybase stuff
#find_package(Sybase)
find_external_library(Sybase
    DYNAMIC_ONLY
    INCLUDES sybdb.h
    LIBS sybblk_r64 sybdb64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64
    HINTS "/opt/sybase/clients/15.7-64bit/OCS-15_0/")
set(SYBASE_DBLIBS "${SYBASE_LIBS}")

############################################################################
#
# FreeTDS
# FIXME: do we need these anymore?
#
set(ftds64   ftds64)
set(FTDS64_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS64_INCLUDE    ${FTDS64_CTLIB_INCLUDE})

set(ftds95   ftds95)
set(FTDS95_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS95_CTLIB_LIB   ct_ftds95 tds_ftds95)
set(FTDS95_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds95/freetds)
set(FTDS95_LIBS        ${FTDS95_CTLIB_LIBS})
set(FTDS95_LIB        ${FTDS95_CTLIB_LIB})
set(FTDS95_INCLUDE    ${FTDS95_CTLIB_INCLUDE})

set(ftds          ftds95)
set(FTDS_LIBS     ${FTDS95_LIBS})
set(FTDS_LIB      ${FTDS95_LIB})
set(FTDS_INCLUDE  ${FTDS95_INCLUDE})

#OpenSSL
find_package(OpenSSL)
if (OpenSSL_FOUND)
    set(OpenSSL_LIBRARIES ${OpenSSL_LIBRARIES} ${Z_LIBS} ${DL_LIBS})
    set(OPENSSL_LIBS ${OpenSSL_LIBRARIES})
endif()

#EXTRALIBS were taken from mysql_config --libs
find_external_library(Mysql INCLUDES mysql/mysql.h LIBS mysqlclient EXTRALIBS ${Z_LIBS} ${CRYPT_LIBS} ${NETWORK_LIBS} ${MATH_LIBS} ${OPENSSL_LIBS})

############################################################################
#
# BerkeleyDB
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.BerkeleyDB.cmake)

# ODBC
# FIXME: replace with native CMake check
#
set(ODBC_INCLUDE  ${includedir}/dbapi/driver/odbc/unix_odbc 
                  ${includedir0}/dbapi/driver/odbc/unix_odbc)
set(ODBC_LIBS)

# Python
find_external_library(Python
    INCLUDES Python.h
    LIBS python2.7
    INCLUDE_HINTS "/opt/python-2.7env/include/python2.7/"
    LIBS_HINTS "/opt/python-2.7env/lib/")

############################################################################
#
# Boost settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.boost.cmake)

############################################################################
#
# NCBI C Toolkit:  headers and libs
string(REGEX MATCH "DNCBI_INT8_GI|NCBI_STRICT_GI" INT8GI_FOUND "${CMAKE_CXX_FLAGS}")
if (NOT "${INT8GI_FOUND}" STREQUAL "")
    if (EXISTS "${NCBI_TOOLS_ROOT}/ncbi/ncbi.gi64/")
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/ncbi.gi64/")
    elseif (EXISTS "${NCBI_TOOLS_ROOT}/ncbi.gi64")
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi.gi64/")
    else ()
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/")
    endif ()
else ()
    set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/")
endif ()

if (EXISTS "${NCBI_CTOOLKIT_PATH}/include64" AND EXISTS "${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_INCLUDE  "${NCBI_CTOOLKIT_PATH}/include64")
    set(NCBI_C_LIBPATH  "-L${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_ncbi     "${NCBI_C_LIBPATH} -lncbi")
    if (APPLE)
        set(NCBI_C_ncbi ${NCBI_C_ncbi} -Wl,-framework,ApplicationServices)
    endif ()
    set(HAVE_NCBI_C true)
else ()
    set(HAVE_NCBI_C false)
endif ()

############################################################################
#
# OpenGL: headers and libs (including core X dependencies) for code
# not using other toolkits.  (The wxWidgets variables already include
# these as appropriate.)

find_package(OpenGL)
if (WIN32)
    set(GLEW_INCLUDE ${WIN32_PACKAGE_ROOT}/glew-1.5.8/include)
    set(GLEW_LIBRARIES ${WIN32_PACKAGE_ROOT}/glew-1.5.8/lib/glew32mx.lib)
else()
    find_package(GLEW)
endif()
find_package(OSMesa)
if (${OPENGL_FOUND})
    set(OPENGL_INCLUDE "${OPENGL_INCLUDE_DIR}")
    set(OPENGL_LIBS "${OPENGL_LIBRARIES}")

    set(OPENGL_INCLUDE ${OPENGL_INCLUDE_DIRS})
    set(OPENGL_LIBS ${OPENGL_LIBRARIES})
    set(GLEW_INCLUDE ${GLEW_INCLUDE_DIRS})
    set(GLEW_LIBS ${GLEW_LIBRARIES})
    set(OSMESA_INCLUDE ${OSMesa_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS})
    set(OSMESA_LIBS ${OSMesa_LIBRARIES} ${OPENGL_LIBRARIES})

endif()

############################################################################
#
# wxWidgets
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.wxwidgets.cmake)

# Fast-CGI
if (APPLE)
  find_external_library(FastCGI
      INCLUDES fastcgi.h
      LIBS fcgi
      HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0")
else ()
    if ("${BUILD_SHARED_LIBS}" STREQUAL "OFF")
        find_external_library(FastCGI
            INCLUDES fastcgi.h
            LIBS fcgi
            INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include"
            LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/lib"
            EXTRALIBS ${NETWORK_LIBS})
    else ()
    find_external_library(FastCGI
        INCLUDES fastcgi.h
        LIBS fcgi
        INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include"
        LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/shlib"
        EXTRALIBS ${NETWORK_LIBS})
  endif()
endif()

# Fast-CGI lib:  (module to add to the "xcgi" library)
set(FASTCGI_OBJS    fcgibuf)

# NCBI SSS:  headers, library path, libraries
set(NCBI_SSS_INCLUDE ${incdir}/sss)
set(NCBI_SSS_LIBPATH )
set(LIBSSSUTILS      -lsssutils)
set(LIBSSSDB         -lsssdb -lssssys)
set(sssutils         sssutils)
set(NCBILS2_LIB      ncbils2_cli ncbils2_asn ncbils2_cmn)
set(NCBILS_LIB       ${NCBILS2_LIB})


# SP:  headers, libraries
set(SP_INCLUDE )
set(SP_LIBS    )

# ORBacus CORBA headers, library path, libraries
set(ORBACUS_INCLUDE )
set(ORBACUS_LIBPATH )
set(LIBOB           )
# LIBIMR should be empty for single-threaded builds
set(LIBIMR          )

# IBM's International Components for Unicode
find_external_library(ICU
    INCLUDES unicode/ucnv.h
    LIBS icui18n icuuc icudata
    HINTS "${NCBI_TOOLS_ROOT}/icu-49.1.1")


##############################################################################
## LibXml2 / LibXsl
##
find_library(GCRYPT_LIBS NAMES gcrypt HINTS "$ENV{NCBI}/libgcrypt/${CMAKE_BUILD_TYPE}/lib")
find_library(GPG_ERROR_LIBS NAMES gpg-error HINTS "$ENV{NCBI}/libgpg-error/${CMAKE_BUILD_TYPE}/lib")
message(STATUS "GCrypt = ${GCRYPT_LIBS}")
message(STATUS "GPG error = ${GPG_ERROR_LIBS}")
if (NOT GCRYPT_LIBS STREQUAL "GCRYPT_LIBS-NOTFOUND")
    set(GCRYPT_FOUND True)
    set(GCRYPT_LIBS ${GCRYPT_LIBS} ${GPG_ERROR_LIBS})
else()
    set(GCRYPT_FOUND False)
endif()

find_package(LibXml2)
if (LIBXML2_FOUND)
    set(LIBXML_INCLUDE ${LIBXML2_INCLUDE_DIR})
    set(LIBXML_LIBS    ${LIBXML2_LIBRARIES})
endif()

find_package(LibXslt)
if (LIBXSLT_FOUND)
    set(LIBXSLT_INCLUDE  ${LIBXSLT_INCLUDE_DIR})
    set(LIBXSLT_LIBS     ${LIBXSLT_EXSLT_LIBRARIES} ${LIBXSLT_LIBRARIES} ${LIBXML_LIBS})
    set(LIBEXSLT_INCLUDE ${LIBXSLT_INCLUDE_DIR})
    set(LIBEXSLT_LIBS    ${LIBXSLT_EXSLT_LIBRARIES})
    if (GCRYPT_FOUND)
        set(LIBXSLT_LIBS     ${LIBXSLT_LIBS} ${GCRYPT_LIBS})
        set(LIBEXSLT_LIBS    ${LIBEXSLT_LIBS} ${GCRYPT_LIBS})
    endif()
endif()


find_external_library(xerces
    INCLUDES xercesc/dom/DOM.hpp
    LIBS xerces-c
    HINTS "${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64")

find_external_library(xalan
    INCLUDES xalanc/XalanTransformer/XalanTransformer.hpp
    LIBS xalan-c xalanMsg
    HINTS "${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64")

# Sun Grid Engine (libdrmaa):
# libpath - /netmnt/uge/lib/lx-amd64/
find_external_library(SGE
    INCLUDES drmaa.h
    LIBS drmaa
    INCLUDE_HINTS "/netmnt/uge/include"
    LIBS_HINTS "/netmnt/uge/lib/lx-amd64/")

# muParser
find_external_library(muparser
    INCLUDES muParser.h
    LIBS muparser
    INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/include"
    LIBS_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/GCC-Debug64/lib/")

# HDF5
find_external_library(hdf5
    INCLUDES hdf5.h
    LIBS hdf5
    HINTS "${NCBI_TOOLS_ROOT}/hdf5-1.8.3/GCC401-Debug64/")

############################################################################
#
# SQLite3
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.sqlite3.cmake)

############################################################################
#
# Various image-format libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.image.cmake)

#############################################################################
## MongoDB

find_library(SASL2_LIBS sasl2)

if (NOT WIN32)
    find_package(MongoCXX)
endif()
if (MONGOCXX_FOUND)
    set(MONGOCXX_INCLUDE ${MONGOCXX_INCLUDE_DIRS} ${BSONCXX_INCLUDE_DIRS})
    set(MONGOCXX_LIB ${MONGOCXX_LIBPATH} ${MONGOCXX_LIBRARIES} ${BSONCXX_LIBRARIES} ${OPENSSL_LIBS} ${SASL2_LIBS})
endif()
message(STATUS "MongoCXX Includes: ${MONGOCXX_INCLUDE}")

## find_external_library(MONGOCXX
##     INCLUDES mongocxx/v_noabi/mongocxx/client.hpp
##     LIBS mongocxx bsoncxx
##     HINTS "${NCBI_TOOLS_ROOT}/mongodb-3.0.2/"
##     EXTRALIBS ${OPENSSL_LIBS} ${SASL2_LIBS})
## set(MONGOCXX_INCLUDE ${MONGOCXX_INCLUDE} "${MONGOCXX_INCLUDE}/mongocxx/v_noabi" "${MONGOCXX_INCLUDE}/bsoncxx/v_noabi")
## set(MONGOCXX_LIB ${MONGOCXX_LIBS})


# libmagic (file-format identification)
find_library(MAGIC_LIBS magic)

# libcurl (URL retrieval)
find_library(CURL_LIBS curl)

# libmimetic (MIME handling)
find_external_library(mimetic
    INCLUDES mimetic/mimetic.h
    LIBS mimetic
    HINTS "${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/")

# libgSOAP++
find_external_library(gsoap
    INCLUDES stdsoap2.h
    LIBS gsoapssl++
    INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/include"
    LIBS_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/lib/"
    EXTRALIBS ${Z_LIBS})

set(GSOAP_SOAPCPP2 ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/soapcpp2)
set(GSOAP_WSDL2H   ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/wsdl2h)

# Compress
set(COMPRESS_LDEP ${CMPRS_LIB})
set(COMPRESS_LIBS xcompress ${COMPRESS_LDEP})


#################################
# Useful sets of object libraries
#################################


set(SOBJMGR_LIBS xobjmgr)
set(ncbi_xreader_pubseqos ncbi_xreader_pubseqos)
set(ncbi_xreader_pubseqos2 ncbi_xreader_pubseqos2)
set(OBJMGR_LIBS ncbi_xloader_genbank)


# Overlapping with qall is poor, so we have a second macro to make it
# easier to stay out of trouble.
set(QOBJMGR_ONLY_LIBS xobjmgr id2 seqsplit id1 genome_collection seqset
    ${SEQ_LIBS} pub medline biblio general-lib xcompress ${CMPRS_LIB})
set(QOBJMGR_LIBS ${QOBJMGR_ONLY_LIBS} qall)
set(QOBJMGR_STATIC_LIBS ${QOBJMGR_ONLY_LIBS} qall)

# EUtils
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout
              einfo uilist ehistory)


#
# SRA/VDB stuff
find_external_library(VDB
    INCLUDES sra/sradb.h
    LIBS ncbi-vdb
    INCLUDE_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/2.8.0/interfaces"
    LIBS_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/2.8.0/linux/release/x86_64/lib")

if (${VDB_FOUND})
    set(VDB_INCLUDE "${VDB_INCLUDE}" "${VDB_INCLUDE}/os/linux" "${VDB_INCLUDE}/os/unix" "${VDB_INCLUDE}/cc/gcc/x86_64" "${VDB_INCLUDE}/cc/gcc")
    set(SRA_INCLUDE ${VDB_INCLUDE})
    set(SRA_SDK_SYSLIBS ${VDB_LIBS})
    set(SRA_SDK_LIBS ${VDB_LIBS})
    set(SRAXF_LIBS ${SRA_SDK_LIBS})
    set(SRA_LIBS ${SRA_SDK_LIBS})
    set(BAM_LIBS ${SRA_SDK_LIBS})
    set(SRAREAD_LDEP ${SRA_SDK_LIBS})
    set(SRAREAD_LIBS sraread ${SRA_SDK_LIBS})
    set(HAVE_NCBI_VDB 1)
endif ()

# Makefile.blast_macros.mk
set(BLAST_DB_DATA_LOADER_LIBS ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt)
set(BLAST_FORMATTER_MINIMAL_LIBS xblastformat align_format taxon1 blastdb_format
    gene_info xalnmgr blastxml xcgi xhtml)
set(BLAST_INPUT_LIBS blastinput
    ${BLAST_DB_DATA_LOADER_LIBS} ${BLAST_FORMATTER_MINIMAL_LIBS})

set(BLAST_FORMATTER_LIBS ${BLAST_INPUT_LIBS})

# Libraries required to link against the internal BLAST SRA library
set(BLAST_SRA_LIBS blast_sra ${SRAXF_LIBS} vxf ${SRA_LIBS})

# BLAST_FORMATTER_LIBS and BLAST_INPUT_LIBS need $BLAST_LIBS
set(BLAST_LIBS xblast xalgoblastdbindex composition_adjustment
xalgodustmask xalgowinmask seqmasks_io seqdb blast_services xobjutil
${OBJREAD_LIBS} xnetblastcli xnetblast blastdb scoremat tables xalnmgr)



# SDBAPI stuff
set(SDBAPI_LIB sdbapi) # ncbi_xdbapi_ftds ${FTDS_LIB} dbapi dbapi_driver ${XCONNEXT})


set(VARSVC_LIBS varsvcutil varsvccli varsvcobj)


# Entrez Libs
set(ENTREZ_LIBS entrez2cli entrez2)
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout einfo uilist ehistory)

#GLPK
find_external_library(glpk
    INCLUDES glpk.h
    LIBS glpk
    HINTS "/usr/local/glpk/4.45")

find_external_library(samtools
    INCLUDES bam.h
    LIBS bam
    HINTS "${NCBI_TOOLS_ROOT}/samtools")

#LAPACK
check_include_file(lapacke.h HAVE_LAPACKE_H)
check_include_file(lapacke/lapacke.h HAVE_LAPACKE_LAPACKE_H)
check_include_file(Accelerate/Accelerate.h HAVE_ACCELERATE_ACCELERATE_H)
#find_external_library(LAPACK LIBS lapack blas)
find_package(LAPACK)
if (LAPACK_FOUND)
    set(LAPACK_INCLUDE ${LAPACK_INCLUDE_DIRS})
    set(LAPACK_LIBS ${LAPACK_LIBRARIES})
else ()
    find_library(LAPACK_LIBS lapack blas)
endif ()

#LMBD
find_external_library(LMDB INCLUDES lmdb.h LIBS lmdb HINTS "${NCBI_TOOLS_ROOT}/lmdb-0.9.18" EXTRALIBS pthread)

find_external_library(libxlsxwriter
    INCLUDES xlsxwriter.h
    LIBS xlsxwriter
    HINTS "${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9"
    EXTRALIBS ${Z_LIBS})

find_external_library(LIBUNWIND INCLUDES libunwind.h LIBS unwind HINTS "${NCBI_TOOLS_ROOT}/libunwind-1.1")
set(HAVE_LIBUNWIND ${LIBUNWIND_FOUND})

##############################################################################
#
# NCBI-isms
# FIXME: these should be tested not hard-coded
if (WIN32)
    set(NCBI_DATATOOL "//snowman/win-coremake/App/Ncbi/cppcore/datatool/msvc/2.16.0/datatool.exe")
else()
    set (NCBI_DATATOOL ${NCBI_TOOLS_ROOT}/bin/datatool)
endif()


#
# Final tasks
#

# This file holds information about the build version
message(STATUS "Generating ${build_root}/inc/ncbi_build_ver.h...")
configure_file(${includedir}/common/ncbi_build_ver.h.in ${includedir}/common/ncbi_build_ver.h)

# OS-specific generated header configs
if (UNIX)
    message(STATUS "Generating ${build_root}/inc/ncbiconf_unix.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/config.cmake.h.in ${build_root}/inc/ncbiconf_unix.h)
endif(UNIX)

if (WIN32)
    message(STATUS "Generating ${build_root}/inc/ncbiconf_msvc.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/config.cmake.h.in ${build_root}/inc/ncbiconf_msvc.h)
    message(STATUS "Generating ${includedir}/common/config/ncbiconf_msvc_site.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/ncbiconf_msvc_site.h.in ${includedir}/common/config/ncbiconf_msvc_site.h)
endif (WIN32)

if (APPLE AND NOT UNIX) #XXX 
    message(STATUS "Generating ${build_root}/inc/ncbiconf_xcode.h...")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/config.cmake.h.in ${build_root}/inc/ncbiconf_xcode.h)
endif (APPLE AND NOT UNIX)

#
# write ncbicfg.c.in
#
# FIXME:
# We need to set these variables to get them into the cfg file:
#  - c_ncbi_runpath
#  - SYBASE_LCL_PATH
#  - SYBASE_PATH
#  - FEATURES
set(c_ncbi_runpath "$ORIGIN/../lib")
set(SYBASE_LCL_PATH ${SYBASE_LIBRARIES})
set(SYBASE_PATH "")
set(FEATURES "")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/corelib/ncbicfg.c.in ${CMAKE_BINARY_DIR}/corelib/ncbicfg.c)

ENABLE_TESTING()
#include_directories(${CPP_INCLUDE_DIR} ${build_root}/inc)
include_directories(${incdir} ${includedir0} ${incinternal})

#
# Dump our final diagnostics
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.final-message.cmake)

