/*
 * $Id$
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
 * Authors:  Colleen Bollin
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_data.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_errors.hpp"
#include "aln_scanner_clustal.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
void
CAlnScannerClustal::ProcessAlignmentFile(
    const CSequenceInfo& sequenceInfo,
    CLineInput& iStr,
    SAlignmentFile& alignInfo)
//  ----------------------------------------------------------------------------
{
    xImportAlignmentData(iStr);
    xVerifyAlignmentData(sequenceInfo);
    xExportAlignmentData(alignInfo);
}


//  ----------------------------------------------------------------------------
void
CAlnScannerClustal::xImportAlignmentData(
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    int cumulative_length = 0;
    int num_sequences = 0;
    bool firstBlock = false;
    bool inBlock = false;

    int  blockLineLength = 0;
    int  blockCount = 0;
    int  numSeqs = 0;
    int  seqCount = 0;

    string line;
    int lineCount(0);

    if (!iStr.ReadLine(line)) {
        // error
    }
    // might want to verify file header
    ++lineCount;

    while (iStr.ReadLine(line)) {
        ++lineCount;

        if (line.empty()) {
            continue;
        }
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            if (inBlock) {
                if (blockCount == 1) {
                    numSeqs = seqCount;
                }
                sResetBlockInfo(
                    seqCount, blockLineLength, inBlock);
            }
            continue;
        }

        if (sIsConservationLine(line)) {
            if (!inBlock) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Expected conservation data at end of block"); 
            }
            if (blockCount == 1) {
                numSeqs = seqCount;
            }
            sResetBlockInfo(seqCount, blockLineLength, inBlock);
            continue;
        }

        // Search for end of block
        vector<string> tokens;
        NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize);
        const auto num_tokens = tokens.size();
        if (num_tokens < 2 || num_tokens > 3) {
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                "In data line, expected seqID followed by sequence data and (optionally) data count"); 
        }

        int seqLength = 0;
        if (num_tokens == 3) {
            seqLength = NStr::StringToInt(tokens[2], NStr::fConvErr_NoThrow);
            if (!seqLength) { 
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data and (optionally) data count"); 
            }
        }

        if (!inBlock) {
            inBlock = true;
            ++blockCount;
        }
        ++seqCount;


        if (!sProcessClustalDataLine(
             tokens, lineCount,
             seqCount,
             numSeqs, 
             seqLength,
             blockCount, 
             blockLineLength)) {
            return;
        }
        mSequences[seqCount-1].push_back({tokens[1], lineCount});
    }
}

//  ----------------------------------------------------------------------------
bool 
CAlnScannerClustal::sIsConservationLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    for (const auto& c : line) {
        if (!isspace(c) &&
            c != ':' &&
            c != '.' &&
            c != '*') {
            return false;
        }
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool
CAlnScannerClustal::sProcessClustalDataLine(
    const vector<string>& tokens,
    const int lineNum,
    const int seqCount,
    const int numSeqs,
    const int seqLength,
    const int blockCount,
    int& blockLineLength)
//  ----------------------------------------------------------------------------
{
    auto seqId = tokens[0];
    if (blockCount == 1) {
        if (find(mSeqIds.begin(), mSeqIds.end(), seqId) != mSeqIds.end()) {
            string description = StrPrintf(
                    "Duplicate ID: \"%s\" has already appeared in this block.", seqId.c_str());
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                description);
        }
        mSeqIds.push_back(seqId);
        mSequences.push_back(vector<TLineInfo>());
    }
    else {
        if (seqCount > numSeqs) {
            string description = StrPrintf(
                "Expected %d sequences, but finding data for for another.",
                numSeqs);
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_BadSequenceCount,
                description);
        }

        if (seqId != mSeqIds[seqCount-1]) {
            string description;
            const auto it = find(mSeqIds.begin(), mSeqIds.end(), seqId);
            if (it == mSeqIds.end()) {
                description = StrPrintf(
                    "Expected %d sequences, but finding data for for another.",
                    numSeqs);
                throw SShowStopper(
                    lineNum,
                    EAlnSubcode::eAlnSubcode_BadSequenceCount,
                    description);
            }
            
            if (distance(mSeqIds.begin(), it) < seqCount-1) {
                description = StrPrintf(
                    "Duplicate ID: \"%s\" has already appeared in this block.",
                    seqId.c_str());
            }
            else {
                description = StrPrintf(
                    "Finding data for sequence \"%s\" out of order.",
                    seqId.c_str());
            }
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                description);
        }
    }
    
    if (seqCount==1) {
        blockLineLength = tokens[1].size();
        return true;
    }

    auto currentLineLength = tokens[1].size();
    if (currentLineLength != blockLineLength) {
        string description = StrPrintf(
            "In data line, expected %d symbols but finding %d",
            blockLineLength,
            currentLineLength);
        throw SShowStopper(
            lineNum,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description);
    }
    return true;
};

END_SCOPE(objects)
END_NCBI_SCOPE
