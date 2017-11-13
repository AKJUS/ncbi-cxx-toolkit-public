/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef WGS_UTILS_H
#define WGS_UTILS_H

#include <string>

#include <corelib/ncbistre.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/general/Object_id.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace wgsparse
{

typedef void (CSeq_id::*TSetSeqIdFunc)(CTextseq_id&);
TSetSeqIdFunc FindSetTextSeqIdFunc(CSeq_id::E_Choice choice);

enum EInputType
{
    eSeqSubmit,
    eBioseqSet,
    eSeqEntry
};


bool GetInputType(const std::string& str, EInputType& type);
bool GetInputTypeFromFile(CNcbiIfstream& stream, EInputType& type);

bool IsPubdescContainsSub(const CPubdesc& pub);

string GetSeqIdStr(const CBioseq& bioseq);

bool GetDescr(const CSeq_entry& entry, const CSeq_descr* &descrs);
bool GetAnnot(const CSeq_entry& entry, const CBioseq::TAnnot* &annot);

bool HasLineage(const string& lineage_str, const string& lineage);

CRef<CSeq_submit> GetSeqSubmit(CNcbiIfstream& in, EInputType type);
string GetSeqSubmitTypeName(EInputType type);
string GetIdStr(const CObject_id& obj_id);

CScope& GetScope();

}

#endif // WGS_UTILS_H
