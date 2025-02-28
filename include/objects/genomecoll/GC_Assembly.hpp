/* $Id$
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
 */

/// @file GC_Assembly.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'genome_collection.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: GC_Assembly_.hpp


#ifndef INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GC_ASSEMBLY_HPP
#define INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GC_ASSEMBLY_HPP

#include <corelib/ncbimtx.hpp>

// generated includes
#include <objects/genomecoll/GC_Assembly_.hpp>

// generated classes
#include <objects/genomecoll/GC_TaggedSequences.hpp>
#include <objects/genomecoll/GC_AssemblyUnit.hpp>
#include <objects/genomecoll/GC_Sequence.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CGC_AssemblyDesc;
class CGC_Sequence;
class CGC_Replicon;

/////////////////////////////////////////////////////////////////////////////
class NCBI_GENOME_COLLECTION_EXPORT CGC_Assembly : public CGC_Assembly_Base
{
    typedef CGC_Assembly_Base Tparent;

public:
    typedef list< CConstRef<CGC_Sequence> >     TSequenceList;
    typedef list< CConstRef<CGC_AssemblyUnit> > TAssemblyUnits;
    typedef list< CConstRef<CGC_Assembly> >     TFullAssemblies;

public:
    // constructor
    CGC_Assembly(void);
    // destructor
    ~CGC_Assembly(void);

    /// Retrieve the release id for this assembly
    int GetReleaseId() const;

    /// Retrieve the accession for this assembly
    string GetAccession() const;

    /// Either accession or submitter-provided id
    string GetBestIdentifier() const;

    /// Retrieve the full set of assembly descriptors
    const CGC_AssemblyDesc& GetDesc() const;

    /// Retrieve the tax-id for this assembly
    TTaxId GetTaxId() const;

    /// Retrieve the name of this assembly
    string GetName() const;

    /// Get full label for assmebly; if this is a unit, full assembly name
    /// followed by unit name
    string GetDisplayName() const;

    /// Retrieve the file-safe version of assembly name, if available;
    /// othwreise default to standard name
    string GetFileSafeName() const;

    /// Get file-safe version of full label for assmebly
    string GetFileSafeDisplayName() const;

    /// Is this assembly a RefSeq assembly?
    bool IsRefSeq() const;

    /// Is this assembly a GenBank assembly?
    bool IsGenBank() const;

    /// Is this a non-nuclear assembly unit?
    bool IsOrganelle() const;

    /// If this is an assembly unit, get unit class
    CGC_AssemblyUnit::TClass GetUnitClass() const;

    /// Generate the internal up-pointers
    void CreateHierarchy(CGC_Assembly *target_set = NULL);

    /// Generate the Seq-id index
    void CreateIndex();

    enum ESubset {
        eChromosome,
        eScaffold,
        eComponent,
        eTopLevel,
        eSubmitterPseudoScaffold,
        eAll
    };
    /// Retrieve a subset of molecules
    ///
    void GetMolecules(TSequenceList& molecules,
                      ESubset        subset) const;

    /// Retrieve a subset of molecules separately for each unit, in the same
    /// order in which the units are returned by GetAssemblyUnits()
    ///
    void GetMoleculesByUnit(vector<TSequenceList>& molecules,
                            ESubset        subset) const;

    /// Retrieve a list of all assembly units contained in this assembly
    TAssemblyUnits GetAssemblyUnits() const;

    /// Retrieve a list of all full assemblies contained in this assembly
    /// Note that, if the assembly is a full assembly, then it will be the only
    /// assembly returned; also, if the assembly is not an assembly set, then
    /// the base assembly will be returned.
    TFullAssemblies GetFullAssemblies() const;

    /// Find all references to a given sequence within an assembly
    void Find(const CSeq_id_Handle& id,
              TSequenceList& sequences) const;

    enum EFindSeqOption {
        eEnforceSingle,
        eChooseAny,
        eChooseBest
    };

    /// Find a single sequence corresponding to the supplied id.
    /// Flag find_option specifies what to do if more than one sequence is found with this id.
    /// - eEnforceSingle: throw an exception. This is the default
    /// - eChooseAny: arbitrarily choose one of the sequences
    /// - eChooseBest: choose the best sequneces available with this id. Criteria are:
    /// -- Choose a sequence from the reference full assembly in preference to a sequence from another full assembly
    /// -- Choose a sequence from the primary unit in preference to a sequence from another unit
    /// -- Choose a top-level sequence in preference to a non-top-level sequence
    /// -- Choose a scaffold in preference to a component
    /// -- If there's more than one sequence that's "best" by the above criteria, choose one arbitrarily
    CConstRef<CGC_Sequence> Find(const CSeq_id_Handle& id,
                                 EFindSeqOption find_option = eEnforceSingle) const;

    /// Returns replicon type, location and role
    void GetRepliconTypeLocRole(const CSeq_id_Handle& id, string& type, string& location, set<int>& role) const;

    /// PreWrite() / PostRead() handle events for indexing of local structures
    void PreWrite() const;
    void PostRead();

    /// Access the top-level target set that this assemhly belongs to
    CConstRef<CGC_Assembly> GetTargetSet() const;

    /// Is this assembly the reference assembly of the target set, or part of it?
    bool IsTargetSetReference() const;

private:
    // Prohibit copy constructor and assignment operator
    CGC_Assembly(const CGC_Assembly& value);
    CGC_Assembly& operator=(const CGC_Assembly& value);

    CMutex m_Mutex;
    typedef map<CSeq_id_Handle, TSequenceList> TSequenceIndex;
    TSequenceIndex m_SequenceMap;

    CGC_Assembly* m_TargetSet;

    /// indexing infrastructure
    void x_Index(CGC_Assembly&     assm,     CGC_Replicon& replicon);
    void x_Index(CGC_Assembly&     assm,     CGC_Sequence& seq);
    void x_Index(CGC_AssemblyUnit& unit,     CGC_Replicon& replicon);
    void x_Index(CGC_AssemblyUnit& unit,     CGC_Sequence& seq);
    void x_Index(CGC_Replicon&     replicon, CGC_Sequence& seq);
    void x_Index(CGC_Sequence&     parent,   CGC_Sequence& seq,
                 CGC_TaggedSequences::TState relation);
    void x_Index(CGC_Sequence& seq,
                 CGC_TaggedSequences::TState relation);

    void x_Index(CGC_Assembly& root);

    const list< CRef< CDbtag > >& x_GetId() const;
    string x_GetSubmitterId() const;
};

/////////////////// CGC_Assembly inline methods


/////////////////// end of CGC_Assembly inline methods

NCBISER_HAVE_PRE_WRITE(CGC_Assembly)
NCBISER_HAVE_POST_READ(CGC_Assembly)


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GC_ASSEMBLY_HPP
/* Original file checksum: lines: 86, chars: 2546, CRC32: 1afb85e */
