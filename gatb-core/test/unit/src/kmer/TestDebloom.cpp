/*****************************************************************************
 *   GATB : Genome Assembly Tool Box                                         *
 *   Copyright (c) 2013                                                      *
 *                                                                           *
 *   GATB is free software; you can redistribute it and/or modify it under   *
 *   the CECILL version 2 License, that is compatible with the GNU General   *
 *   Public License                                                          *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
 *   CECILL version 2 License for more details.                              *
 *****************************************************************************/

#include <CppunitCommon.hpp>

#include <gatb/system/impl/System.hpp>

#include <gatb/bank/impl/BankStrings.hpp>

#include <gatb/kmer/impl/DSKAlgorithm.hpp>
#include <gatb/kmer/impl/DebloomAlgorithm.hpp>

#include <gatb/tools/misc/api/Macros.hpp>
#include <gatb/tools/misc/impl/Property.hpp>

#include <gatb/tools/math/NativeInt64.hpp>
#include <gatb/tools/math/NativeInt128.hpp>
#include <gatb/tools/math/LargeInt.hpp>

#include <gatb/tools/collections/impl/Product.hpp>
#include <gatb/tools/collections/impl/CollectionFile.hpp>

using namespace std;

using namespace gatb::core::system;
using namespace gatb::core::system::impl;

using namespace gatb::core::bank;
using namespace gatb::core::bank::impl;

using namespace gatb::core::kmer;
using namespace gatb::core::kmer::impl;

using namespace gatb::core::tools::dp;

using namespace gatb::core::tools::collections;
using namespace gatb::core::tools::collections::impl;

using namespace gatb::core::tools::math;
using namespace gatb::core::tools::misc;
using namespace gatb::core::tools::misc::impl;

/********************************************************************************/
namespace gatb  {  namespace tests  {
/********************************************************************************/

/** \brief Test class for genomic databases management
 */
class TestDebloom : public Test
{
    /********************************************************************************/
    CPPUNIT_TEST_SUITE_GATB (TestDebloom);

        CPPUNIT_TEST_GATB (Debloom_check1);

    CPPUNIT_TEST_SUITE_GATB_END();

public:

    /********************************************************************************/
    void setUp    () {}
    void tearDown () {}


    /********************************************************************************/
    void Debloom_check1 ()
    {
        size_t kmerSize = 11;
        size_t nks      = 1;

        const char* seqs[] = {
            "CGCTACAGCAGCTAGTTCATCATTGTTTATCAATGATAAAATATAATAAGCTAAAAGGAAACTATAAATA"
            "ACCATGTATAATTATAAGTAGGTACCTATTTTTTTATTTTAAACTGAAATTCAATATTATATAGGCAAAG"
        } ;

        /** We create a product instance. */
        Product<CollectionFile> product ("test");

        /** We create a DSK instance. */
        DSKAlgorithm<NativeInt64> dsk (product, new BankStrings (seqs, ARRAY_SIZE(seqs)), kmerSize, nks);

        /** We launch DSK. */
        dsk.execute();

        /** We create a debloom instance. */
        DebloomAlgorithm<NativeInt64> debloom (product, dsk.getSolidKmers(), kmerSize, BloomFactory::Synchronized);

        /** We launch the debloom. */
        debloom.execute();

        /** The following values have been computed with the original minia. */
        u_int64_t values[] =
        {
            0xc0620,    0x288f40,   0x188f40,   0x2aaa29,   0x8000b,    0x200881,   0x288081,   0x820db,    0x52e23,    0x2888f,
            0xaaa8b,    0x28838d,   0x20000,    0xa93ab,    0x2c18d,    0x2ba89,    0x183600,   0xea00b,    0x1a4ea0,   0xf8585
        };
        set<NativeInt64> okValues (values, values + ARRAY_SIZE(values));

        /** We iterate the cFP kmers. */
        set<NativeInt64> checkValues;
        Iterator<NativeInt64>* iter = debloom.getCriticalKmers()->iterator();
        LOCAL (iter);

        for (iter->first(); !iter->isDone(); iter->next())
        {
            set<NativeInt64>::iterator lookup = okValues.find (iter->item());
            CPPUNIT_ASSERT (lookup != okValues.end());

            checkValues.insert (iter->item());
        }

        CPPUNIT_ASSERT (checkValues.size() == okValues.size());
    }
};

/********************************************************************************/

CPPUNIT_TEST_SUITE_REGISTRATION      (TestDebloom);
CPPUNIT_TEST_SUITE_REGISTRATION_GATB (TestDebloom);

/********************************************************************************/
} } /* end of namespaces. */
/********************************************************************************/

