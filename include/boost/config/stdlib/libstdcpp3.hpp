#ifndef NCBI_BOOST_CONFIG_STDLIB___LIBSTDCPP3__HPP
#define NCBI_BOOST_CONFIG_STDLIB___LIBSTDCPP3__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Aaron Ucko
 *
 */

/// @file libstdcpp3.hpp
/// Simple wrapper around Boost's real libstdcpp3.hpp to help catch
/// accidental version skew.

#ifdef __GNUC__
#  pragma GCC system_header
#  include_next <boost/config/stdlib/libstdcpp3.hpp>
#else
#  include <boost/test/../config/stdlib/libstdcpp3.hpp>
#endif
#include <common/boost_skew_guard.hpp>

#endif  /* NCBI_BOOST_CONFIG_STDLIB___LIBSTDCPP3__HPP */
