/* xmlmisc.h
 *
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
 * File Name:  xmlmisc.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
 *      XML functionality from C-toolkit.
 */

#ifndef XMLMISC_H
#define XMLMISC_H

#include <objtools/flatfile/valnode.h>

/* Simple XML Parsing */

typedef struct xmlobj {
    Nlm_CharPtr    name;
    Nlm_CharPtr    contents;
    Nlm_Int2       level;
    struct xmlobj  *attributes;
    struct xmlobj  *children;
    struct xmlobj  *next;
    struct xmlobj  *parent;
    struct xmlobj  *successor;        /* linearizes a recursive exploration */
} Nlm_XmlObj, PNTR Nlm_XmlObjPtr;

#define XmlObj Nlm_XmlObj
#define XmlObjPtr Nlm_XmlObjPtr

typedef void(*VisitXmlNodeFunc) (Nlm_XmlObjPtr xop, Nlm_XmlObjPtr parent, Nlm_Int2 level, Nlm_VoidPtr userdata);

Nlm_XmlObjPtr ParseXmlString(const Char* str);
Nlm_XmlObjPtr FreeXmlObject(Nlm_XmlObjPtr xop);
Nlm_Int4 VisitXmlNodes(Nlm_XmlObjPtr xop, Nlm_VoidPtr userdata, VisitXmlNodeFunc callback, Nlm_CharPtr nodeFilter,
                       Nlm_CharPtr parentFilter, Nlm_CharPtr attrTagFilter, Nlm_CharPtr attrValFilter, Nlm_Int2 maxDepth);

#endif
