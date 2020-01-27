
#include "proc_fig0ext6.h"
#include "fibbits.h"

#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>

#define DEBUG_CMP_TERRIFICVIEW      0
#define PRINT_CEI_LINKS             0
#define PRINT_SVC_LINK_REPETITIONS  0


int numFig0ext6 = 0;
int numDifferenServiceLinkInfos = 0;

struct LinkingInfoVal
{
  LinkingInfoVal()
  {
    for (int k = 0; k < 3; ++k )
    {
      SID[k] = 0;
      num[k] = 0;
      ILS[k] = 0;
      PD[k]  = 0;
      SHD[k] = 0;
    }
  }

  //uint8_t IdLQ;  // 2 bit: 0: DAB SIDs, 1: RDS PIs, 2: reserved, 3: DRM/AMSS SID
  std::unordered_map<uint32_t, unsigned> ID[3];  // 0: DAB, 1: PS, 2: AM
  uint32_t SID[3];     // DAB_SID for ..
  uint32_t num[3];     // # of elements in ID[3][16]
  uint8_t ILS[3];
  uint8_t PD[3];    // 1 bit
  uint8_t SHD[3];    // 1 bit
};

static bool initLinkInfo = true;
static std::pair<std::string, std::string> lastLinkInfos[16];
static int lastLinkInfoAge[16];
static int lastLinkInfoUniqueID[16];
static bool lastLinkInfoValid[16];
static std::ostringstream linkInfo_ostrA;
static std::ostringstream linkInfo_ostrB;
static std::ostringstream linkInfo_ostrC;
static std::string linkInfo_strA;
static std::string linkInfo_strB;
static std::string linkInfo_strC;
static unsigned lastLinkfibNoStart;
static unsigned lastLinkfibNoEnd;
static unsigned lastLinkfibNoCount;
static bool linkInfo_gotFromBegin = false;
static bool linkInfo_first_is_SID = false;
static std::unordered_map<uint32_t, LinkingInfoVal> aLinkInfoMap[2];
static int currLinkInfoMapIdx = 0;


static void initLinkInfoVars()
{
    for ( int k = 0; k < 16; ++k ) {
        lastLinkInfoAge[k] = 0;
        lastLinkInfoUniqueID[k] = -1;
        lastLinkInfoValid[k] = false;
    }

    std::ostringstream tmpE1, tmpE2, tmpE3;
    std::swap( linkInfo_ostrA, tmpE1 );
    std::swap( linkInfo_ostrB, tmpE2 );
    std::swap( linkInfo_ostrC, tmpE3 );
    linkInfo_strA.clear();
    linkInfo_strB.clear();
    linkInfo_strC.clear();
    lastLinkfibNoStart = 0;
    lastLinkfibNoEnd = 0;
    lastLinkfibNoCount = 0;
    linkInfo_gotFromBegin = false;
    linkInfo_first_is_SID = false;
    aLinkInfoMap[0].clear();
    aLinkInfoMap[1].clear();
    currLinkInfoMapIdx = 0;
    initLinkInfo = false;
}

// return idx of new element; -1 if already existed
static int linkInfoWrite()
{
    int retIdx = -1;
    int wrIdx = -1;
    int maxAge = -1;

    if ( linkInfo_strA.length() <= 0 || linkInfo_strB.length() <= 0 )
        return retIdx;

    for ( int k = 0; k < 16; ++k )
    {
        if ( !lastLinkInfoValid[k] )
            continue;
        if ( lastLinkInfos[k].first == linkInfo_strA
          && lastLinkInfos[k].second == linkInfo_strB )
        {
            wrIdx = k;
            //lastLinkInfoAge[wrIdx] = 0;  // refresh age - after output!
            retIdx = -wrIdx -2;
            break;
        }
    }

    if ( wrIdx < 0 ) {
        // now, look for free or oldest entry
        for ( int k = 0; k < 16; ++k )
        {
            if ( !lastLinkInfoValid[k] )
            {
                wrIdx = k;
                break;
            }
            if ( wrIdx < 0 || lastLinkInfoAge[k] > maxAge ) {
                maxAge = lastLinkInfoAge[k];
                wrIdx = k;
            }
        }

        retIdx = wrIdx;
        lastLinkInfoValid[wrIdx] = true;
        lastLinkInfoAge[wrIdx] = 0;
        lastLinkInfos[wrIdx] = std::make_pair( linkInfo_strA, linkInfo_strB );
    }

    // increment age of all other elements
    for ( int k = 0; k < 16; ++k ) {
        if ( k != wrIdx && lastLinkInfoAge[k] < 65536 )
            ++lastLinkInfoAge[k];
    }
    return retIdx;
}

void flush_fig0_ext6()
{
    fprintf(stderr, "received %d FIG0/6, unique %d\n", numFig0ext6, numDifferenServiceLinkInfos);
    int linkNo = 1;

    for( const auto& n : aLinkInfoMap[currLinkInfoMapIdx] )
    {
        const uint32_t lnkKey = n.first;
        const LinkingInfoVal & lnkVal = n.second;
        const unsigned LSN = lnkKey & 0xFFFF;
        const unsigned LA = (lnkKey >> 16) & 0x1;
        const unsigned SH = (lnkKey >> 24) & 0x1;

        fprintf(stderr, "\nLink %d: %s, %s link, LSN: 0x%X\n"
          , linkNo
          , (LA ? "  Active":"Inactive")
          , (SH ? "Hard":"Soft")
          , LSN
          );
        ++linkNo;
        if ( lnkVal.ID[0].size() ) {
            fprintf(stderr, "  DAB SID=0x%X, %s: ", lnkVal.SID[0], (lnkVal.ILS[0] ? "Intl":"Nat."));
            for( const auto& k : lnkVal.ID[0] )
                fprintf(stderr, "0x%X, ", k.first);
            fprintf(stderr, "\n");
        }
        if ( lnkVal.ID[1].size() ) {
            fprintf(stderr, "  PS  SID=0x%X, %s: ", lnkVal.SID[1], (lnkVal.ILS[1] ? "Intl":"Nat."));
            for( const auto& k : lnkVal.ID[1] )
                fprintf(stderr, "0x%X, ", k.first);
            fprintf(stderr, "\n");
        }
        if ( lnkVal.ID[2].size() ) {
            fprintf(stderr, "  AM  SID=0x%X, %s: ", lnkVal.SID[2], (lnkVal.ILS[2] ? "Intl":"Nat."));
            for( const auto& k : lnkVal.ID[2] )
                fprintf(stderr, "0x%X, ", k.first);
            fprintf(stderr, "\n");
        }
    }

  //uint8_t IdLQ;  // 2 bit: 0: DAB SIDs, 1: RDS PIs, 2: reserved, 3: DRM/AMSS SID
  //uint32_t ID[3][16];  // 0: DAB, 1: PS, 2: AM
  //uint32_t SID[3];     // DAB_SID for ..
  //uint32_t num[3];     // # of elements in ID[3][16]
  //uint8_t ILS[3];
  //uint8_t PD[3];    // 1 bit
  //uint8_t SHD[3];    // 1 bit

  aLinkInfoMap[currLinkInfoMapIdx].clear();
}

void proc_fig0_ext6(const uint8_t * figData, int figLen, unsigned fibCallbackNo)
{
    if ( initLinkInfo )
        initLinkInfoVars();

    ++numFig0ext6;
    //fprintf(stderr, "FIG 0/6: # %d len %d @fib %u\n", numFig0ext6, figLen, fibCallbackNo);

    // common for FIG type 0
    const uint8_t CN = getSingleBit( 0, figData );
    const uint8_t OE = getSingleBit( 1, figData );
    const uint8_t PD = getSingleBit( 2, figData );
    //const uint8_t fig0Ext = getBits8( 5, 3, figData );

    if ( CN == 0 )  // new DB_STRT?
    {
        linkInfo_strB = linkInfo_ostrB.str();

        int createdIdx = linkInfoWrite();
        if ( createdIdx >= 0 && linkInfo_gotFromBegin ) {
#if DEBUG_CMP_TERRIFICVIEW
            linkInfo_strC = linkInfo_ostrC.str();
#endif
            ++numDifferenServiceLinkInfos;
            lastLinkInfoUniqueID[createdIdx] = numDifferenServiceLinkInfos;
#if DEBUG_CMP_TERRIFICVIEW
            fprintf(stderr, "\nSERVICE LINKING INFO #%d: %s, FIB#%u:%u..%u\n%s\n%s\n"
                    , numDifferenServiceLinkInfos
                    , lastLinkInfos[createdIdx].first.c_str()
                    , lastLinkfibNoCount, lastLinkfibNoStart, lastLinkfibNoEnd
                    , lastLinkInfos[createdIdx].second.c_str()
                    , linkInfo_strC.c_str()
                    );
#else
            fprintf(stderr, "\nSERVICE LINKING INFO #%d: %s, FIB#%u:%u..%u\n%s\n"
                    , numDifferenServiceLinkInfos
                    , lastLinkInfos[createdIdx].first.c_str()
                    , lastLinkfibNoCount, lastLinkfibNoStart, lastLinkfibNoEnd
                    , lastLinkInfos[createdIdx].second.c_str()
                    );
#endif
        }
        else if ( createdIdx < -1 )
        {
            int idx = -2 -createdIdx;
#if PRINT_SVC_LINK_REPETITIONS
            fprintf(stderr, "\nSERVICE LINKING INFO repeat #%d age %d, FIB %u .. %u\n"
                    , lastLinkInfoUniqueID[idx], 1+lastLinkInfoAge[idx]
                    , lastLinkfibNoStart, lastLinkfibNoEnd );
#endif
            lastLinkInfoAge[idx] = 0;  // refresh age - after output!
        }
        // prepare for next
        std::ostringstream tmpE2, tmpE3;
        std::swap( linkInfo_ostrB, tmpE2 );
#if DEBUG_CMP_TERRIFICVIEW
        std::swap( linkInfo_ostrC, tmpE3 );
#endif
        linkInfo_strA.clear();
        linkInfo_strB.clear();
#if DEBUG_CMP_TERRIFICVIEW
        linkInfo_strC.clear();
#endif
        lastLinkfibNoStart = lastLinkfibNoEnd = fibCallbackNo;
        lastLinkfibNoCount = 1;
        linkInfo_first_is_SID = true;
        linkInfo_gotFromBegin = true;
    }

    std::ostringstream tmpE1;
    std::swap( linkInfo_ostrA, tmpE1 );
    linkInfo_ostrA << "OE " << (OE ? "OTHR":"THIS") << " Ensemble, " << (PD ? "32/data":"16/program" );
    if ( linkInfo_strA.length() <= 0 )
        linkInfo_strA = linkInfo_ostrA.str();
    else if ( linkInfo_strA != linkInfo_ostrA.str() )
        linkInfo_ostrB << linkInfo_ostrA.str();

    // to content of FIG type 0 "field"
    --figLen;
    ++figData;

    while (figLen >= 2)
    {
        int incLen = 0;
        const uint8_t id_list_flag = getSingleBit( 0, figData );
        const uint8_t LA = getSingleBit( 1, figData );
        const uint8_t SH = getSingleBit( 2, figData );
        const uint8_t ILS = getSingleBit( 3, figData );
        const uint16_t LSN = getMax16Bits( 12, 4, figData );

        if ( lastLinkfibNoEnd != fibCallbackNo )
            ++lastLinkfibNoCount;
        lastLinkfibNoEnd = fibCallbackNo;

        if ( PRINT_CEI_LINKS || id_list_flag )
            linkInfo_ostrB << "Link: " << (LA ? "  Active":"Inactive") << ", "
                           << (SH ? "Hard":"Soft") << " link, "
                           << (ILS ? "Intl":"Nat.") << " link, "
                           << "LSN: 0x" << std::hex << std::uppercase << (unsigned)LSN
                           << ( id_list_flag ? "\n" : ", CEI\n" ); // Change Event Indication

#if DEBUG_CMP_TERRIFICVIEW
        uint8_t num_ids = id_list_flag ? getMax8Bits( 4, 20, figData ) : 0;
        linkInfo_ostrC << "FIB#" << fibCallbackNo << ": CN " << (int)CN << ", OE " << (int)OE << ", PD " << (int)PD
                       << ", idList " << (int)id_list_flag << ", LA " << (int)LA << ", SH " << (int)SH << ", ILS " << (int)ILS
                       << ", numIds " << (int)num_ids << "\n";
#endif

        if ( id_list_flag )
        {
            const uint8_t id_list_usage = getMax8Bits( 4, 16, figData );
            const uint8_t Rfu  = (id_list_usage) & 8;  // must be 0. else definition of IdLQ and ID List is invalid
            const uint8_t IdLQ = (id_list_usage >> 1) & 3;
            if ( Rfu == 0 && IdLQ != 2 )
            {
                // ETSI EN 300 401 V1.4.1 (2006-01): SHD = Shorthand for  PD==0
                // ETSI EN 300 401 V2.1.1 (2016-10): RFa = Reserver for Future Additions
                const uint8_t SHD = (id_list_usage) & 1;
                const char * pacIdLQ = nullptr;
                switch (IdLQ)
                {
                case 0: pacIdLQ = "DAB SIDs";  break;
                case 1: pacIdLQ = "RDS PIs";   break;
                case 2: pacIdLQ = "reserved";  break;
                case 3: pacIdLQ = "DRM/AMSS SID";  break;
                }

                const uint32_t lnkKey = uint32_t(LSN) | (uint32_t(LA) << 16) | (uint32_t(SH) << 24);
                LinkingInfoVal &lnkVal = aLinkInfoMap[currLinkInfoMapIdx][lnkKey];
                const int lnkIdx = (IdLQ > 2) ? 2 : IdLQ;
                std::unordered_map<uint32_t, unsigned> & IDLIST = lnkVal.ID[lnkIdx];

                uint8_t num_ids = getMax8Bits( 4, 20, figData );
                if (num_ids)
                {
                    lnkVal.ILS[lnkIdx] = ILS;
                    lnkVal.PD[lnkIdx] = PD;
                    lnkVal.SHD[lnkIdx] = SHD;
                }
                if ( PD == 1 )
                {
                    linkInfo_ostrB << "  " << pacIdLQ << " 32Bit:"
                                   << ( num_ids ? " ":"\n");
                    for ( uint8_t k = 0U; k < num_ids; ++k )
                    {
                        uint32_t id = getMax32Bits(32, 24+k*32, figData);
                        linkInfo_ostrB << "0x" << std::hex << std::uppercase << id
                                       << ( (k == 0 && linkInfo_first_is_SID) ? " (=DAB_SID)":"" )
                                       << (k+1 == num_ids ? "\n":", ");
                        if (linkInfo_first_is_SID)
                          lnkVal.SID[lnkIdx] = id;
                        else
                          IDLIST[id]++;
                        linkInfo_first_is_SID = false;
                    }
                    incLen += 3 + 4*num_ids;
                    figLen -= 3 + 4*num_ids;
                    figData += 3 + 4*num_ids;
                }
                else if ( ILS == 1 )
                {
                    linkInfo_ostrB << "  SHD-" << std::dec << (int)SHD << ", " << pacIdLQ << ", ECC/ID 8/16-Bit:"
                                   << ( num_ids ? " ":"\n");
                    for ( uint8_t k = 0U; k < num_ids; ++k )
                    {
                        uint16_t ecc = getMax8Bits(8, 24+k*24, figData);
                        uint16_t id = getMax16Bits(16, 24+k*24+8, figData);
                        linkInfo_ostrB << "0x" << std::hex << std::uppercase << (unsigned)ecc << "/0x" << std::hex << (unsigned)id
                                       << ( (k == 0 && linkInfo_first_is_SID) ? " (=DAB_SID)":"" )
                                       << (k+1 == num_ids ? "\n":", ");
                        if (linkInfo_first_is_SID)
                          lnkVal.SID[lnkIdx] = ((uint32_t)ecc) << 16 | (uint32_t)id;
                        else
                          IDLIST[((uint32_t)ecc) << 16 | (uint32_t)id]++;
                        linkInfo_first_is_SID = false;
                    }
                    incLen += 3 + 3*num_ids;
                    figLen -= 3 + 3*num_ids;
                    figData += 3 + 3*num_ids;
                }
                else // if ( ILS == 0 )
                {
                    linkInfo_ostrB << "  SHD-" << std::dec << (int)SHD << ", " << pacIdLQ << ", ID 16-Bit:"
                                   << ( num_ids ? " ":"\n");
                    for ( uint8_t k = 0U; k < num_ids; ++k )
                    {
                        uint16_t id = getMax16Bits(16, 24+k*16, figData);
                        linkInfo_ostrB << "0x" << std::hex << std::uppercase << (unsigned)id
                                       << ( (k == 0 && linkInfo_first_is_SID) ? " (=DAB_SID)":"" )
                                       << (k+1 == num_ids ? "\n":", ");
                        if (linkInfo_first_is_SID)
                          lnkVal.SID[lnkIdx] = id;
                        else
                          IDLIST[id]++;
                        linkInfo_first_is_SID = false;
                    }
                    incLen += 3 + 2*num_ids;
                    figLen -= 3 + 2*num_ids;
                    figData += 3 + 2*num_ids;
                }
            }
        }
        else
        {
            incLen += 2;
            figLen -= 2;
            figData += 2;
        }
    }
}

