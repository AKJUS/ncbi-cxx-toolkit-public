/* loadfeat.h
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
 * File Name:  loadfeat.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef _LOADFEAT_
#define _LOADFEAT_

#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/flatfile/xgbfeat.h>

#include <objtools/flatfile/asci_blk.h>

typedef struct feature_block {
    Int4      num;
    CharPtr   key;
    CharPtr   location;

    TQualVector quals;

    feature_block() :
        num(0),
        key(NULL),
        location(NULL)
    {
    }

} FeatBlk, PNTR FeatBlkPtr;

void LoadFeat PROTO((ParserPtr pp, DataBlkPtr entry, ncbi::objects::CBioseq& bioseq));
int  ParseFeatureBlock PROTO((IndexblkPtr ibp, bool deb, DataBlkPtr dbp, Int2 source, Int2 format));

void GetFlatBiomol PROTO((int& biomol, Uint1 tech, CharPtr molstr, ParserPtr pp, DataBlkPtr entry, const ncbi::objects::COrg_ref* org_ref));

bool GetSeqLocation(ncbi::objects::CSeq_feat& feat, CharPtr location, TSeqIdList& ids,
                    bool PNTR hard_err, ParserPtr pp, CharPtr name);

#endif
