#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <core/debug/debug.hpp>

#include <components/FastCMPCache/AbstractDirectory.hpp>
#include <components/FastCMPCache/SharingVector.hpp>
#include <components/FastCMPCache/AbstractProtocol.hpp>
#include <components/FastCMPCache/BlockDirectoryEntry.hpp>
#include <unordered_map>

#include <boost/dynamic_bitset.hpp>

#include <list>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/list.hpp>

#include <components/CommonQEMU/Serializers.hpp>
#include <components/CommonQEMU/Util.hpp>

using nCommonSerializers::StdDirEntrySerializer;
using nCommonUtil::log_base2;

namespace nFastCMPCache {

typedef BlockDirectoryEntry BlockDirectoryEntry;

struct AddrHash {
  std::size_t operator()(const PhysicalMemoryAddress & addr) const {
    return addr >> 6;
  }
};
typedef std::unordered_map<PhysicalMemoryAddress, BlockDirectoryEntry_p, AddrHash> inf_directory_t;

struct TaglessDirectoryBucket : public AbstractDirectoryEntry {
  BlockDirectoryEntry theTaglessEntry;
  inf_directory_t  thePreciseDirectory;
};

struct TaglessLookupResult : public AbstractDirectoryEntry {
  std::list<TaglessDirectoryBucket *> theBuckets;
  BlockDirectoryEntry_p theTrueState;
  BlockDirectoryEntry theTaglessState;
};

typedef boost::intrusive_ptr<TaglessLookupResult> TaglessLookupResult_p;

class TaglessDirectory : public AbstractDirectory {
private:
  bool theTrackCollisions;
  bool theTrackBitCounts;
  bool theTrackBitPatterns;
  bool theUpdateOnSnoop;
  bool thePartitioned;

  int32_t theNumSets;
  int32_t theNumBuckets;
  int32_t theTotalNumBuckets;

  int32_t theSetShift;
  int32_t theSetMask;

  std::vector<std::vector<TaglessDirectoryBucket> > theDirectory;

  TaglessDirectory() : AbstractDirectory(), theTrackCollisions(false), theTrackBitCounts(false), theTrackBitPatterns(false), thePartitioned(true) {};

  typedef std::function<int(uint64_t)> hash_fn_t;

  std::list<hash_fn_t> hash_fns;

  int32_t theHashShift;
  int32_t theHashMask;
  int32_t theHashBits;
  int32_t theTagBits;
  int32_t theHashXORShift;
  int32_t theRotation;
  int32_t theFreeBits;
  int32_t theNumPatterns;

  Flexus::Stat::StatCounter * theDestructiveCollisions;
  Flexus::Stat::StatCounter * theConstructiveCollisions;
  Flexus::Stat::StatCounter * theNonCollisions;

#define DEFINE_COLLISION_COUNTERS(type)           \
 Flexus::Stat::StatCounter *the ## type ## Collisions_All;     \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OnChip_Read;   \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OnChip_Fetch;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OnChip_Write;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OnChip_Upgrade;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OnChip_Evict;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OnChip_Other;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OffChip_Read;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OffChip_Fetch;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OffChip_Write;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OffChip_Upgrade;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OffChip_Evict;  \
 Flexus::Stat::StatCounter *the ## type ## Collisions_OffChip_Other;

  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_All;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OnChip_Read;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OnChip_Fetch;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OnChip_Write;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OnChip_Upgrade;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OnChip_Evict;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OnChip_Other;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OffChip_Read;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OffChip_Fetch;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OffChip_Write;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OffChip_Upgrade;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OffChip_Evict;
  Flexus::Stat::StatInstanceCounter<int64_t> *theExtraBits_OffChip_Other;

  DEFINE_COLLISION_COUNTERS(Non)
  DEFINE_COLLISION_COUNTERS(Destructive)
  DEFINE_COLLISION_COUNTERS(Constructive)

  Flexus::Stat::StatCounter ** theBitCounters;

  Flexus::Stat::StatCounter ** *theBitPatternCounters;

  Flexus::Stat::StatInstanceCounter<int64_t> *thePerSetCollisions;
  Flexus::Stat::StatInstanceCounter<int64_t> *thePerBucketCollisions;

  enum HashPolicy {
    eSimpleHash,
    eXORHash,
    eMyHash,
    eOverlapHash,
    ePrimeModuloHash,
    eRotatedPrimeHash,
    eFullPrimeHash
  };

  std::list<std::string> theHashPolicyList;

#define INITIALIZE_COLLISION_COUNTERS(type)                       \
 the ## type ## Collisions_All = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":All");       \
 the ## type ## Collisions_OnChip_Read = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OnChip:Read");   \
 the ## type ## Collisions_OnChip_Fetch = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OnChip:Fetch");  \
 the ## type ## Collisions_OnChip_Write = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OnChip:Write");  \
 the ## type ## Collisions_OnChip_Upgrade = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OnChip:Upgrade"); \
 the ## type ## Collisions_OnChip_Evict = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OnChip:Evict");  \
 the ## type ## Collisions_OnChip_Other = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OnChip:Other");  \
 the ## type ## Collisions_OffChip_Read = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OffChip:Read");  \
 the ## type ## Collisions_OffChip_Fetch = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OffChip:Fetch");  \
 the ## type ## Collisions_OffChip_Write = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OffChip:Write");  \
 the ## type ## Collisions_OffChip_Upgrade = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OffChip:Upgrade"); \
 the ## type ## Collisions_OffChip_Evict = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OffChip:Evict");  \
 the ## type ## Collisions_OffChip_Other = new Flexus::Stat::StatCounter(aName + "-Collisions:" + #type + ":OffChip:Other");

  virtual void initialize(const std::string & aName) {

    AbstractDirectory::initialize(aName);

    DBG_Assert( theNumSets > 0 );
    DBG_Assert( theNumBuckets > 0 );
    DBG_Assert( (theNumSets & (theNumSets - 1)) == 0, ( << "theNumSets = " << theNumSets << " != Power of 2" ) );

    DBG_Assert( theHashPolicyList.size() > 0, ( << "No hash policy given." ));

    if (thePartitioned) {
      theTotalNumBuckets = theHashPolicyList.size() * theNumBuckets;
    } else {
      theTotalNumBuckets = theNumBuckets;
    }

    theDirectory.resize(theNumSets, std::vector<TaglessDirectoryBucket>(theTotalNumBuckets, TaglessDirectoryBucket() ));

    theSetMask = theNumSets - 1;
    theSetShift = log_base2(theBlockSize);

    theHashShift = log_base2(theNumSets) + theSetShift;
    theHashMask = theNumBuckets - 1;
    theHashBits = log_base2(theNumBuckets);
    theTagBits = 34 - theHashBits;

    DBG_(Dev, ( << "Created Tagless Directory with " << theNumSets << " sets and " << theTotalNumBuckets << (thePartitioned ? " partitioned " : " ") << "buckets" ));
    DBG_(Dev, ( << "SetShift = " << theSetShift << ", HashShift = " << theHashShift << ", SetMask = " << std::hex << theSetMask << ", HashMask = " << theHashMask ));
    std::list<std::string>::iterator iter = theHashPolicyList.begin();
    for (; iter != theHashPolicyList.end(); iter++) {
      createHashPolicy(*iter);
    }

    // Initialize Stats
    if (theTrackCollisions) {
      INITIALIZE_COLLISION_COUNTERS(Non)
      INITIALIZE_COLLISION_COUNTERS(Destructive)
      INITIALIZE_COLLISION_COUNTERS(Constructive)

      theExtraBits_All = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:All");
      theExtraBits_OnChip_Read = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OnChip:Read");
      theExtraBits_OnChip_Fetch = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OnChip:Fetch");
      theExtraBits_OnChip_Write = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OnChip:Write");
      theExtraBits_OnChip_Upgrade = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OnChip:Upgrade");
      theExtraBits_OnChip_Evict = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OnChip:Evict");
      theExtraBits_OnChip_Other = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OnChip:Other");
      theExtraBits_OffChip_Read = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OffChip:Read");
      theExtraBits_OffChip_Fetch = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OffChip:Fetch");
      theExtraBits_OffChip_Write = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OffChip:Write");
      theExtraBits_OffChip_Upgrade = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OffChip:Upgrade");
      theExtraBits_OffChip_Evict = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OffChip:Evict");
      theExtraBits_OffChip_Other = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-ExtraBits:OffChip:Other");

      thePerSetCollisions = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-Collisions:PerSet");
      thePerBucketCollisions = new Flexus::Stat::StatInstanceCounter<int64_t>(aName + "-Collisions:PerBucket");

      // How many "free" bits are there?
      theFreeBits = 32 - theHashShift;

      if (theTrackBitCounts) {
        theBitCounters = new Flexus::Stat::StatCounter*[theFreeBits];
        for (int32_t i = 0; i < theFreeBits; i++) {
          theBitCounters[i] = new Flexus::Stat::StatCounter(aName + "-BitUsed:" + std::to_string(i));
        }
      }

      if (theTrackBitPatterns) {
        theNumPatterns = theFreeBits - theHashBits + 1;
        theBitPatternCounters = new Flexus::Stat::StatCounter ** [theNumPatterns];
        for (int32_t i = 0; i < theNumPatterns; i++) {
          theBitPatternCounters[i] = new Flexus::Stat::StatCounter*[theNumBuckets];
          for (int32_t j = 0; j < theNumBuckets; j++) {
            theBitPatternCounters[i][j] = new Flexus::Stat::StatCounter(aName + "-BitPattern:"
                + std::to_string(i) + ":" + std::to_string(j) );
          }
        }
      }
    }
  }

#define RECORD_COLLISION_FN(type) \
 void record ## type ## Collision(MMType req_type, bool on_chip) {         \
  (*the ## type ## Collisions_All)++;                \
  if (on_chip) {                     \
   switch(req_type) {                   \
   case MemoryMessage::ReadReq: (*the ## type ## Collisions_OnChip_Read)++; break;  \
   case MemoryMessage::FetchReq: (*the ## type ## Collisions_OnChip_Fetch)++; break; \
   case MemoryMessage::UpgradeReq: (*the ## type ## Collisions_OnChip_Upgrade)++; break; \
   case MemoryMessage::WriteReq: (*the ## type ## Collisions_OnChip_Write)++; break; \
                          \
   case MemoryMessage::EvictClean:                \
   case MemoryMessage::EvictWritable:               \
   case MemoryMessage::EvictDirty:                \
    (*the ## type ## Collisions_OnChip_Evict)++;              \
    break;                     \
                          \
   default:                     \
    (*the ## type ## Collisions_OnChip_Other)++;              \
    break;                     \
   }                       \
  } else {                      \
   switch(req_type) {                   \
   case MemoryMessage::ReadReq: (*the ## type ## Collisions_OffChip_Read)++; break; \
   case MemoryMessage::FetchReq: (*the ## type ## Collisions_OffChip_Fetch)++; break; \
   case MemoryMessage::UpgradeReq: (*the ## type ## Collisions_OffChip_Upgrade)++; break; \
   case MemoryMessage::WriteReq: (*the ## type ## Collisions_OffChip_Write)++; break; \
                          \
   case MemoryMessage::EvictClean:                \
   case MemoryMessage::EvictWritable:               \
   case MemoryMessage::EvictDirty:                \
    (*the ## type ## Collisions_OffChip_Evict)++;           \
    break;                     \
                          \
   default:                     \
    (*the ## type ## Collisions_OffChip_Other)++;           \
    break;                     \
   }                       \
  }                        \
 }

  RECORD_COLLISION_FN(Non)
  RECORD_COLLISION_FN(Constructive)
  RECORD_COLLISION_FN(Destructive)

  void recordExtraBits(MMType req_type, bool on_chip, int32_t bits) {
    if (on_chip) {
      switch (req_type) {
        case MemoryMessage::ReadReq:
          (*theExtraBits_OnChip_Read) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::FetchReq:
          (*theExtraBits_OnChip_Fetch) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::UpgradeReq:
          (*theExtraBits_OnChip_Upgrade) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::WriteReq:
          (*theExtraBits_OnChip_Write) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::EvictClean:
        case MemoryMessage::EvictWritable:
        case MemoryMessage::EvictDirty:
          (*theExtraBits_OnChip_Evict) << std::make_pair(bits, 1);
          break;
        default:
          (*theExtraBits_OnChip_Other) << std::make_pair(bits, 1);
          break;
      }
    } else {
      switch (req_type) {
        case MemoryMessage::ReadReq:
          (*theExtraBits_OffChip_Read) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::FetchReq:
          (*theExtraBits_OffChip_Fetch) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::UpgradeReq:
          (*theExtraBits_OffChip_Upgrade) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::WriteReq:
          (*theExtraBits_OffChip_Write) << std::make_pair(bits, 1);
          break;
        case MemoryMessage::EvictClean:
        case MemoryMessage::EvictWritable:
        case MemoryMessage::EvictDirty:
          (*theExtraBits_OffChip_Evict) << std::make_pair(bits, 1);
          break;
        default:
          (*theExtraBits_OffChip_Other) << std::make_pair(bits, 1);
          break;
      }
    }
  }

  inline int32_t get_set(PhysicalMemoryAddress addr) {
    return ((uint64_t)addr >> theSetShift) & theSetMask;
  }

  int32_t get_next_prime(int32_t num) {
    static const int32_t prime_list[] = {
      2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97,
      101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
      233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379,
      383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
      547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691,
      701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857, 859, 863,
      877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019, 1021, 1031, 1033, 1039,
      1049, 1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097, 1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163, 1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
      1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283, 1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321, 1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423, 1427,
      1429, 1433, 1439, 1447, 1451, 1453, 1459, 1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511, 1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571, 1579, 1583,
      1597, 1601, 1607, 1609, 1613, 1619, 1621, 1627, 1637, 1657, 1663, 1667, 1669, 1693, 1697, 1699, 1709, 1721, 1723, 1733, 1741, 1747, 1753, 1759, 1777,
      1783, 1787, 1789, 1801, 1811, 1823, 1831, 1847, 1861, 1867, 1871, 1873, 1877, 1879, 1889, 1901, 1907, 1913, 1931, 1933, 1949, 1951, 1973, 1979, 1987,
      1993, 1997, 1999, 2003, 2011, 2017, 2027, 2029, 2039, 2053, 2063, 2069, 2081, 2083, 2087, 2089, 2099, 2111, 2113, 2129, 2131, 2137, 2141, 2143, 2153,
      2161, 2179, 2203, 2207, 2213, 2221, 2237, 2239, 2243, 2251, 2267, 2269, 2273, 2281, 2287, 2293, 2297, 2309, 2311, 2333, 2339, 2341, 2347, 2351, 2357,
      2371, 2377, 2381, 2383, 2389, 2393, 2399, 2411, 2417, 2423, 2437, 2441, 2447, 2459, 2467, 2473, 2477, 2503, 2521, 2531, 2539, 2543, 2549, 2551, 2557,
      2579, 2591, 2593, 2609, 2617, 2621, 2633, 2647, 2657, 2659, 2663, 2671, 2677, 2683, 2687, 2689, 2693, 2699, 2707, 2711, 2713, 2719, 2729, 2731, 2741,
      2749, 2753, 2767, 2777, 2789, 2791, 2797, 2801, 2803, 2819, 2833, 2837, 2843, 2851, 2857, 2861, 2879, 2887, 2897, 2903, 2909, 2917, 2927, 2939, 2953,
      2957, 2963, 2969, 2971, 2999, 3001, 3011, 3019, 3023, 3037, 3041, 3049, 3061, 3067, 3079, 3083, 3089, 3109, 3119, 3121, 3137, 3163, 3167, 3169, 3181,
      3187, 3191, 3203, 3209, 3217, 3221, 3229, 3251, 3253, 3257, 3259, 3271, 3299, 3301, 3307, 3313, 3319, 3323, 3329, 3331, 3343, 3347, 3359, 3361, 3371,
      3373, 3389, 3391, 3407, 3413, 433, 3449, 3457, 3461, 3463, 3467, 3469, 3491, 3499, 3511, 3517, 3527, 3529, 3533, 3539, 3541, 3547, 3557, 3559, 3571,
      3581, 3583, 3593, 3607, 3613, 3617, 3623, 3631, 3637, 3643, 3659, 3671, 3673, 3677, 3691, 3697, 3701, 3709, 3719, 3727, 3733, 3739, 3761, 3767, 3769,
      3779, 3793, 3797, 3803, 3821, 3823, 3833, 3847, 3851, 3853, 3863, 3877, 3881, 3889, 3907, 3911, 3917, 3919, 3923, 3929, 3931, 3943, 3947, 3967, 3989,
      4001, 4003, 4007, 4013, 4019, 4021, 4027, 4049, 4051, 4057, 4073, 4079, 4091, 4093, 4099, 4111, 4127, 4129, 4133, 4139, 4153, 4157, 4159, 4177, 4201,
      4211, 4217, 4219, 4229, 4231, 4241, 4243, 4253, 4259, 4261, 4271, 4273, 4283, 4289, 4297, 4327, 4337, 4339, 4349, 4357, 4363, 4373, 4391, 4397, 4409,
      4421, 4423, 4441, 4447, 4451, 4457, 4463, 4481, 4483, 4493, 4507, 4513, 4517, 4519, 4523, 4547, 4549, 4561, 4567, 4583, 4591, 4597, 4603, 4621, 4637,
      4639, 4643, 4649, 4651, 4657, 4663, 4673, 4679, 4691, 4703, 4721, 4723, 4729, 4733, 4751, 4759, 4783, 4787, 4789, 4793, 4799, 4801, 4813, 4817, 4831,
      4861, 4871, 4877, 4889, 4903, 4909, 4919, 4931, 4933, 4937, 4943, 4951, 4957, 4967, 4969, 4973, 4987, 4993, 4999, 5003, 5009, 5011, 5021, 5023, 5039,
      5051, 5059, 5077, 5081, 5087, 5099, 5101, 5107, 5113, 5119, 5147, 5153, 5167, 5171, 5179, 5189, 5197, 5209, 5227, 5231, 5233, 5237, 5261, 5273, 5279,
      5281, 5297, 5303, 5309, 5323, 5333, 5347, 5351, 5381, 5387, 5393, 5399, 5407, 5413, 5417, 5419, 5431, 5437, 5441, 5443, 5449, 5471, 5477, 5479, 5483,
      5501, 5503, 5507, 5519, 5521, 5527, 5531, 5557, 5563, 5569, 5573, 5581, 5591, 5623, 5639, 5641, 5647, 5651, 5653, 5657, 5659, 5669, 5683, 5689, 5693,
      5701, 5711, 5717, 5737, 5741, 5743, 5749, 5779, 5783, 5791, 5801, 5807, 5813, 5821, 5827, 5839, 5843, 5849, 5851, 5857, 5861, 5867, 5869, 5879, 5881,
      5897, 5903, 5923, 5927, 5939, 5953, 5981, 5987, 6007, 6011, 6029, 6037, 6043, 6047, 6053, 6067, 6073, 6079, 6089, 6091, 6101, 6113, 6121, 6131, 6133,
      6143, 6151, 6163, 6173, 6197, 6199, 6203, 6211, 6217, 6221, 6229, 6247, 6257, 6263, 6269, 6271, 6277, 6287, 6299, 6301, 6311, 6317, 6323, 6329, 6337,
      6343, 6353, 6359, 6361, 6367, 6373, 6379, 6389, 6397, 6421, 6427, 6449, 6451, 6469, 6473, 6481, 6491, 6521, 6529, 6547, 6551, 6553, 6563, 6569, 6571,
      6577, 6581, 6599, 6607, 6619, 6637, 6653, 6659, 6661, 6673, 6679, 6689, 6691, 6701, 6703, 6709, 6719, 6733, 6737, 6761, 6763, 6779, 6781, 6791, 6793,
      6803, 6823, 6827, 6829, 6833, 6841, 6857, 6863, 6869, 6871, 6883, 6899, 6907, 6911, 6917, 6947, 6949, 6959, 6961, 6967, 6971, 6977, 6983, 6991, 6997,
      7001, 7013, 7019, 7027, 7039, 7043, 7057, 7069, 7079, 7103, 7109, 7121, 7127, 7129, 7151, 7159, 7177, 7187, 7193, 7207, 7211, 7213, 7219, 7229, 7237,
      7243, 7247, 7253, 7283, 7297, 7307, 7309, 7321, 7331, 7333, 7349, 7351, 7369, 7393, 7411, 7417, 7433, 7451, 7457, 7459, 7477, 7481, 7487, 7489, 7499,
      7507, 7517, 7523, 7529, 7537, 7541, 7547, 7549, 7559, 7561, 7573, 7577, 7583, 7589, 7591, 7603, 7607, 7621, 7639, 7643, 7649, 7669, 7673, 7681, 7687,
      7691, 7699, 7703, 7717, 7723, 7727, 7741, 7753, 7757, 7759, 7789, 7793, 7817, 7823, 7829, 7841, 7853, 7867, 7873, 7877, 7879, 7883, 7901, 7907, 7919
    };
    int32_t i;
    for (i = 0; i < 2000; i++) {
      if (num < prime_list[i]) {
        break;
      }
    }
    if (i == 0) {
      return num;
    } else {
      return prime_list[i - 1];
    }
  }

  int32_t simple_hash(uint64_t addr) {
    return (addr >> theHashShift) & theHashMask;
  }

  int32_t shift_hash(int32_t shift, uint64_t addr) {
    return (addr >> (theHashShift + shift)) & theHashMask;
  }

  int32_t xor_hash(uint64_t addr) {
    return ((addr >> theHashShift) ^ (addr >> theHashXORShift)) & theHashMask;
  }

  int32_t prime_modulo_hash(uint64_t addr) {
    return ((addr >> theHashShift) % theNumBuckets);
  }

  int32_t full_prime_modulo_hash(uint64_t addr) {
    return (addr % theNumBuckets);
  }

  int32_t rotated_prime_modulo_hash(uint64_t addr) {
    uint32_t x = addr;
    x = ((x >> (32 - theRotation)) & ((1 << theRotation) - 1)) | (x << theRotation);
    return (x % theNumBuckets);
  }

  int32_t my_hash(uint64_t addr) {
    int32_t a = (addr >> (16 + 12)) & 0xF;
    int32_t b = (addr >> (16 + 8)) & 0xF;
    int32_t c = (addr >> (16 + 4)) & 0xF;
    int32_t d = (addr >> (16 + 0)) & 0xF;
    return ((c << 6) | (d << 4) | (b << 1) | a) & theHashMask;
  }

  int32_t overlap_hash(uint64_t addr) {
    int32_t ret = 0;
    addr >>= theHashShift;
    int32_t bit_mask = 1;
    for (int32_t i = 0; i < theHashBits; i++) {
      ret ^= (addr & bit_mask);
      bit_mask <<= 1;
      ret |= (addr & bit_mask);
      addr >>= 1;
    }
    return (ret & theHashMask);
  }

  struct MatrixHash {
  public:
    MatrixHash(const std::list<int> &matrix, int32_t shift, int32_t mask) : theMatrix(matrix), theHashShift(shift), theHashMask(mask) {}
    int32_t operator()(uint64_t addr) {
      addr >>= theHashShift;
      int32_t ret = 0;
      std::list<int>::iterator it = theMatrix.begin();
      for (; it != theMatrix.end(); it++, addr >>= 1) {
        if ((addr & 1) != 0) {
          ret ^= *it;
        }
      }
      return (ret & theHashMask);
    }
  private:
    std::list<int> theMatrix;
    int    theHashShift;
    int    theHashMask;
  };

  // A List of Matrix Hash objects to use
  std::list<MatrixHash> theMatrixHashes;

  static HashPolicy string2HashPolicy(const std::string & policy) {
    if (strcasecmp(policy.c_str(), "simple") == 0) {
      return eSimpleHash;
    } else if (strcasecmp(policy.c_str(), "xor") == 0) {
      return eXORHash;
    } else if (strcasecmp(policy.c_str(), "prime") == 0) {
      return ePrimeModuloHash;
    } else if (strcasecmp(policy.c_str(), "full_prime") == 0) {
      return eFullPrimeHash;
    } else if (strcasecmp(policy.c_str(), "rotated_prime") == 0) {
      return eRotatedPrimeHash;
    } else if (strcasecmp(policy.c_str(), "my_hash") == 0) {
      return eMyHash;
    } else if (strcasecmp(policy.c_str(), "overlap") == 0) {
      return eOverlapHash;
    }
    DBG_(Dev, ( << "Unknown hash policy '" << policy << "' using SimpleHash." ));
    return eSimpleHash;
  }

  void createHashPolicy(std::string & policy) {

    DBG_(Dev, ( << "creating hash '" << policy << "'" ));

    if (strcasecmp(policy.c_str(), "simple") == 0) {
      DBG_Assert( (theNumBuckets & (theNumBuckets - 1)) == 0, ( << "theNumBuckets = " << theNumBuckets << " != Power of 2" ) );
      hash_fns.push_back([this](uint64_t addr){return this->simple_hash(addr);});
    } else if (strcasecmp(policy.c_str(), "rotated_prime") == 0) {
      hash_fns.push_back([this](uint64_t addr){return this->rotated_prime_modulo_hash(addr);});
      theNumBuckets = get_next_prime(theNumBuckets);
    } else if (strcasecmp(policy.c_str(), "full_prime") == 0) {
      hash_fns.push_back([this](uint64_t addr){return this->full_prime_modulo_hash(addr);});
      theNumBuckets = get_next_prime(theNumBuckets);
    } else if (strcasecmp(policy.c_str(), "prime") == 0) {
      hash_fns.push_back([this](uint64_t addr){return this->prime_modulo_hash(addr);});
      theNumBuckets = get_next_prime(theNumBuckets);
    } else if (strcasecmp(policy.c_str(), "my_hash") == 0) {
      DBG_Assert( (theNumBuckets & (theNumBuckets - 1)) == 0, ( << "theNumBuckets = " << theNumBuckets << " != Power of 2" ) );
      hash_fns.push_back([this](uint64_t addr){return this->my_hash(addr);});
    } else if (strcasecmp(policy.c_str(), "overlap") == 0) {
      DBG_Assert( (theNumBuckets & (theNumBuckets - 1)) == 0, ( << "theNumBuckets = " << theNumBuckets << " != Power of 2" ) );
      hash_fns.push_back([this](uint64_t addr){return this->overlap_hash(addr);});
    } else if (strcasecmp(policy.c_str(), "xor") == 0) {
      DBG_Assert( (theNumBuckets & (theNumBuckets - 1)) == 0, ( << "theNumBuckets = " << theNumBuckets << " != Power of 2" ) );
      hash_fns.push_back([this](uint64_t addr){return this->xor_hash(addr);});
      if (theHashXORShift <= 0) {
        theHashXORShift = 32 - log_base2(theNumBuckets);
      }
    } else if (strncasecmp(policy.c_str(), "shift", 5) == 0) {
      DBG_Assert( (theNumBuckets & (theNumBuckets - 1)) == 0, ( << "theNumBuckets = " << theNumBuckets << " != Power of 2" ) );
      int32_t shift = boost::lexical_cast<int>(policy.substr(6));
      hash_fns.push_back([this, shift](uint64_t addr){return this->shift_hash(shift, addr);});
    } else if (strncasecmp(policy.c_str(), "matrix", 6) == 0) {
      std::string matrix_type = policy.substr(7);
      std::list<int> matrix;
      if (strncasecmp(matrix_type.c_str(), "random", 6) == 0) {
        uint32_t seed = boost::lexical_cast<uint32_t>(matrix_type.substr(7));
        std::srand(seed);
        int32_t n = theTagBits;
        double max = theNumBuckets;
        // Generate a list of n numbers with theHashBits bits
        for (int32_t i = 0; i < n; i++) {
          int32_t random = (int)(max * std::rand() / (RAND_MAX + 1.0));
          matrix.push_back(random);
        }
      } else if (strncasecmp(matrix_type.c_str(), "list", 4) == 0) {
        std::string matrix_list = matrix_type.substr(5);
        // The matrix is a list of comma-separated numbers
        std::string::size_type pos = 0;
        int32_t num = 0;
        do {
          pos = matrix_list.find(',', 0);
          if (pos == std::string::npos) {
            num = boost::lexical_cast<int>(matrix_list);
          } else {
            num = boost::lexical_cast<int>(matrix_list.substr(0, pos));
            matrix_list = matrix_list.substr(pos + 1);
          }
          matrix.push_back(num);
        } while (pos != std::string::npos);
      } else {
        DBG_Assert(false, ( << "Unknown Matrix Hash Type '" << matrix_type << "'" ));
      }
      theMatrixHashes.push_back(MatrixHash(matrix, theHashShift, theHashMask));
      hash_fns.push_back(theMatrixHashes.back());//boost::ref(theMatrixHashes.back()));
    } else {
      DBG_Assert(false, ( << "Unknown Hash Policy '" << policy << "'" ));
    }
    DBG_(Dev, ( << "Creating hash policy: " << policy ));
  }

  void addHashPolicy(std::string policy) {
    theHashPolicyList.push_back(policy);
  }

protected:

  virtual void addSharer(int32_t index, AbstractEntry_p dir_entry, PhysicalMemoryAddress address) {
    TaglessLookupResult * my_entry = dynamic_cast<TaglessLookupResult *>(dir_entry.get());

    DBG_Assert(my_entry != nullptr);
    std::list<TaglessDirectoryBucket *>::iterator bucket_iter = my_entry->theBuckets.begin();
    for (; bucket_iter != my_entry->theBuckets.end(); bucket_iter++) {
      (*bucket_iter)->theTaglessEntry.addSharer(index);
    }
    my_entry->theTaglessState.addSharer(index);
    my_entry->theTrueState->addSharer(index);
  }

  virtual void addExclusiveSharer(int32_t index, AbstractEntry_p dir_entry, PhysicalMemoryAddress address) {
    TaglessLookupResult * my_entry = dynamic_cast<TaglessLookupResult *>(dir_entry.get());
    DBG_Assert(my_entry != nullptr);

    std::list<TaglessDirectoryBucket *>::iterator bucket_iter = my_entry->theBuckets.begin();
    for (; bucket_iter != my_entry->theBuckets.end(); bucket_iter++) {
      (*bucket_iter)->theTaglessEntry.addSharer(index);
    }
    my_entry->theTaglessState.addSharer(index);
    my_entry->theTrueState->addSharer(index);
    my_entry->theTrueState->makeExclusive(index);
  }

  virtual void failedSnoop(int32_t index, AbstractEntry_p dir_entry, PhysicalMemoryAddress address) {
    // We track precise sharers, so a failed snoop means we remove a sharer
    removeSharer(index, dir_entry, address);
    // we don't remove sharers here, only on evicts

  }

  virtual void removeSharer(int32_t index, AbstractEntry_p dir_entry, PhysicalMemoryAddress address) {
    //BlockDirectoryEntry *my_entry = dynamic_cast<BlockDirectoryEntry*>(dir_entry);
    //if (my_entry == nullptr) {
    // return;
    //}
    //my_entry->removeSharer(index);

    TaglessLookupResult * my_entry = dynamic_cast<TaglessLookupResult *>(dir_entry.get());
    DBG_Assert(my_entry != nullptr);

    my_entry->theTrueState->removeSharer(index);

    if (theUpdateOnSnoop) {
      RegionScoutMessage probe(RegionScoutMessage::eSetTagProbe, PhysicalMemoryAddress(address));
      thePorts.sendRegionProbe(probe, index);

      std::list<hash_fn_t>::iterator hash_iter = hash_fns.begin();
      std::list<TaglessDirectoryBucket *>::iterator bucket_iter = my_entry->theBuckets.begin();
      int32_t bucket_offset = ( thePartitioned ? theNumBuckets : 0 );
      int32_t first_bucket = 0;
      for (; hash_iter != hash_fns.end(); hash_iter++, bucket_iter++, first_bucket += bucket_offset) {
        int32_t my_bucket = (*hash_iter)(address) + first_bucket;

        std::list<PhysicalMemoryAddress>::iterator iter = probe.getTags().begin();
        std::list<PhysicalMemoryAddress>::iterator end = probe.getTags().end();
        for (; iter != end; iter++) {
          if (*iter == address) {
            continue;
          }
          if (thePartitioned) {
            int32_t bucket = (*hash_iter)(*iter) + first_bucket;
            if (bucket == my_bucket) {
              break;
            }
          } else {
            // Not partitioned, have to check ALL hash functions for each block
            std::list<hash_fn_t>::iterator test_hash = hash_fns.begin();
            for (; test_hash != hash_fns.end(); test_hash++) {
              int32_t bucket = (*test_hash)(*iter);
              if (bucket == my_bucket) {
                break;
              }
            }
            if (test_hash != hash_fns.end()) {
              break;
            }
          }
        }
        if (iter == end) {
          (*bucket_iter)->theTaglessEntry.removeSharer(index);
        }
      }
    }
  }

  virtual void makeSharerExclusive(int32_t index, AbstractEntry_p dir_entry, PhysicalMemoryAddress address) {
    //BlockDirectoryEntry *my_entry = dynamic_cast<BlockDirectoryEntry*>(dir_entry);
    //if (my_entry == nullptr) {
    // return;
    //}
    // Make it exclusive
    //my_entry->makeExclusive(index);

    TaglessLookupResult * my_entry = dynamic_cast<TaglessLookupResult *>(dir_entry.get());
    DBG_Assert(my_entry != nullptr);

    my_entry->theTrueState->makeExclusive(index);
  }

  TaglessLookupResult_p findOrCreateEntry(PhysicalMemoryAddress addr, int32_t index) {

    TaglessLookupResult_p result(new TaglessLookupResult());

    int32_t set_index = get_set(addr);
    DBG_Assert( (set_index >= 0) && (set_index < theNumSets), ( << "Invalid set: " << set_index << " for addr " << std::hex << addr ));

    std::list<hash_fn_t>::iterator hash_iter = hash_fns.begin();
    int32_t bucket_offset = ( thePartitioned ? theNumBuckets : 0 );
    int32_t first_bucket = 0;
    for (; hash_iter != hash_fns.end(); hash_iter++, first_bucket += bucket_offset) {
      int32_t bucket_index = (*hash_iter)(addr) + first_bucket;

      DBG_Assert( (bucket_index >= first_bucket) && (bucket_index < theTotalNumBuckets), ( << "Invalid bucket: " << bucket_index << " for addr " << std::hex << addr ));

      TaglessDirectoryBucket * bucket = &(theDirectory[set_index][bucket_index]);
      result->theBuckets.push_back(bucket);
    }

    DBG_Assert(result->theBuckets.size() > 0);

    // Do we have a Precise entry for this block?
    inf_directory_t::iterator iter = result->theBuckets.front()->thePreciseDirectory.find(addr);
    if (iter == result->theBuckets.front()->thePreciseDirectory.end()) {
      // Need to create new entry
      BlockDirectoryEntry_p block(new BlockDirectoryEntry(addr));
      std::list<TaglessDirectoryBucket *>::iterator bucket_iter = result->theBuckets.begin();
      for (; bucket_iter != result->theBuckets.end(); bucket_iter++) {
        (*bucket_iter)->thePreciseDirectory.insert(std::make_pair(addr, block));
      }
      result->theTrueState = block;
    } else {
      result->theTrueState = iter->second;
    }

    result->theTaglessState = result->theBuckets.front()->theTaglessEntry;
    std::list<TaglessDirectoryBucket *>::iterator bucket_iter = result->theBuckets.begin();
    bucket_iter++;
    for (; bucket_iter != result->theBuckets.end(); bucket_iter++) {
      result->theTaglessState &= (*bucket_iter)->theTaglessEntry;
    }

    return result;
  }

  TaglessLookupResult_p findEntry(PhysicalMemoryAddress addr, int32_t index) {

    TaglessLookupResult_p result(new TaglessLookupResult());

    int32_t set_index = get_set(addr);
    DBG_Assert( (set_index >= 0) && (set_index < theNumSets), ( << "Invalid set: " << set_index << " for addr " << std::hex << addr ));

    std::list<hash_fn_t>::iterator hash_iter = hash_fns.begin();
    int32_t bucket_offset = ( thePartitioned ? theNumBuckets : 0 );
    int32_t first_bucket = 0;
    for (; hash_iter != hash_fns.end(); hash_iter++, first_bucket += bucket_offset) {
      int32_t bucket_index = (*hash_iter)(addr) + first_bucket;

      DBG_Assert( (bucket_index >= first_bucket) && (bucket_index < theTotalNumBuckets), ( << "Invalid bucket: " << bucket_index << " for addr " << std::hex << addr ));

      TaglessDirectoryBucket * bucket = &(theDirectory[set_index][bucket_index]);
      result->theBuckets.push_back(bucket);
    }

    DBG_Assert(result->theBuckets.size() > 0);

    // Do we have a Precise entry for this block?
    inf_directory_t::iterator iter = result->theBuckets.front()->thePreciseDirectory.find(addr);
    if (iter == result->theBuckets.front()->thePreciseDirectory.end()) {
      // Don't bother adding TrueState
    } else {
      result->theTrueState = iter->second;
    }

    result->theTaglessState = result->theBuckets.front()->theTaglessEntry;
    std::list<TaglessDirectoryBucket *>::iterator bucket_iter = result->theBuckets.begin();
    bucket_iter++;
    for (; bucket_iter != result->theBuckets.end(); bucket_iter++) {
      result->theTaglessState &= (*bucket_iter)->theTaglessEntry;
    }

    return result;
  }

protected:

  virtual void processRequestResponse(int32_t index, const MMType & request, MMType & response,
                                      AbstractEntry_p dir_entry, PhysicalMemoryAddress address, bool off_chip) {
    // First, do default behaviour
    AbstractDirectory::processRequestResponse(index, request, response, dir_entry, address, off_chip);

    if (!MemoryMessage::isEvictType(request)) {
      return;
    }

    TaglessLookupResult * my_entry = dynamic_cast<TaglessLookupResult *>(dir_entry.get());
    if (my_entry == nullptr) {
      return;
    }

    // If there are no sharers, remove precise dir entries
    if (my_entry->theTrueState->state() == ZeroSharers) {
      std::list<TaglessDirectoryBucket *>::iterator bucket_iter = my_entry->theBuckets.begin();
      for (; bucket_iter != my_entry->theBuckets.end(); bucket_iter++) {
        (*bucket_iter)->thePreciseDirectory.erase(address);
      }
    }

    // Check if the evicting node has any other blocks that map to the same bucket

    RegionScoutMessage probe(RegionScoutMessage::eSetTagProbe, PhysicalMemoryAddress(address));
    thePorts.sendRegionProbe(probe, index);

    std::list<hash_fn_t>::iterator hash_iter = hash_fns.begin();
    std::list<TaglessDirectoryBucket *>::iterator bucket_iter = my_entry->theBuckets.begin();
    int32_t bucket_offset = ( thePartitioned ? theNumBuckets : 0 );
    int32_t first_bucket = 0;

    // If not partitioned, we have to make sure none of the hash functions map to the same bucket
    for (; hash_iter != hash_fns.end(); hash_iter++, bucket_iter++, first_bucket += bucket_offset) {
      int32_t my_bucket = (*hash_iter)(address) + first_bucket;

      std::list<PhysicalMemoryAddress>::iterator iter = probe.getTags().begin();
      std::list<PhysicalMemoryAddress>::iterator end = probe.getTags().end();
      for (; iter != end; iter++) {
        if (*iter == address) {
          continue;
        }
        if (thePartitioned) {
          int32_t bucket = (*hash_iter)(*iter) + first_bucket;
          if (bucket == my_bucket) {
            break;
          }
        } else {
          // Not partitioned, have to check ALL hash functions for each block
          std::list<hash_fn_t>::iterator test_hash = hash_fns.begin();
          for (; test_hash != hash_fns.end(); test_hash++) {
            int32_t bucket = (*test_hash)(*iter);
            if (bucket == my_bucket) {
              break;
            }
          }
          if (test_hash != hash_fns.end()) {
            break;
          }
        }
      }
      if (iter == end) {
        (*bucket_iter)->theTaglessEntry.removeSharer(index);
      }
    }
  }

public:
  virtual std::tuple<SharingVector, SharingState, AbstractEntry_p>
  lookup(int32_t index, PhysicalMemoryAddress address, MMType req_type, std::list<std::function<void(void)> > &xtra_actions) {

    TaglessLookupResult_p block = findOrCreateEntry(address, index);

    if (theTrackCollisions) {
      // How many extra bits are set
      std::bitset<MAX_NUM_SHARERS> extra_bits = block->theTaglessState.sharers().getSharers();
      extra_bits &= ~(block->theTrueState->sharers().getSharers());
      extra_bits[index] = false;

      int32_t num_extra_bits = extra_bits.count();

      bool on_chip = ((block->theTrueState->state() == ManySharers) || ((block->theTrueState->state() == OneSharer) && (!block->theTrueState->sharers().isSharer(index))));

      (*theExtraBits_All) << std::make_pair(num_extra_bits, 1);
      recordExtraBits(req_type, on_chip, num_extra_bits);

      // Determine if this was a collision
      std::list<TaglessDirectoryBucket *>::iterator bucket_iter = block->theBuckets.begin();
      for (; bucket_iter != block->theBuckets.end(); bucket_iter++) {
        if ((*bucket_iter)->thePreciseDirectory.size() == 1) {
          break;
        }
      }

      if (bucket_iter != block->theBuckets.end()) {
        recordNonCollision(req_type, on_chip);
        DBG_Assert(num_extra_bits == 0, ( << "No Collision found, BUT num_extra_bits = " << num_extra_bits << ", tagless state = " << block->theTaglessState.sharers() << ", true state = " << block->theTrueState->sharers() ));
      } else {
        // was this a destructive or constructive collision?
        // Do we have the right state?
        if (num_extra_bits == 0) {
          recordConstructiveCollision(req_type, on_chip);
        } else {
          recordDestructiveCollision(req_type, on_chip);
        }
      }

      if (theTrackBitCounts) {
        uint64_t mask = (1 << theHashShift);
        for (int32_t i = 0; i < theFreeBits; i++, mask <<= 1) {
          if ((address & mask) != 0) {
            (*theBitCounters[i])++;
          }
        }
      }

      if (theTrackBitPatterns) {
        uint64_t val = address >> theHashShift;
        for (int32_t i = 0; i < theNumPatterns; i++, val >>= 1) {
          (*theBitPatternCounters[i][val & theHashMask])++;
        }
      }
    }

    DBG_(Trace, ( << "Received " << req_type << " request from core " << index << " for block " << std::hex << address << " Tagless state = " << block->theTaglessState.sharers() << ", TueState = " << block->theTrueState->sharers() ));

    return std::make_tuple(block->theTaglessState.sharers(), block->theTaglessState.state(), block);
  }

  virtual std::tuple<SharingVector, SharingState, AbstractEntry_p, bool>
  snoopLookup(int32_t index, PhysicalMemoryAddress address, MMType req_type) {

    TaglessLookupResult_p block = findEntry(address, index);

    DBG_(Trace, ( << "Received " << req_type << " request from core " << index << " for block " << std::hex << address << " Tagless state = " << block->theTaglessState.sharers() << ", TueState = " << block->theTrueState->sharers() ));

    bool valid = true;
    return std::tie(block->theTaglessState.sharers(), block->theTaglessState.state(), block, valid);
  }
  void saveState( std::ostream & s, const std::string & aDirName ) {
    boost::archive::binary_oarchive oa(s);

    int32_t tmp = theNumSets;
    oa << tmp;
    tmp = theNumBuckets;
    oa << tmp;

    for (int32_t set = 0; set < theNumSets; set++) {
      for (int32_t bucket = 0; bucket < theNumBuckets; bucket++) {
        StdDirEntrySerializer serializer(theDirectory[set][bucket].theTaglessEntry.getSerializer());
        oa << serializer;
      }
    }
  }

  bool loadState( std::istream & s, const std::string & aDirName ) {
    boost::archive::binary_iarchive ia(s);

    int32_t sets, buckets;
    ia >> sets;
    ia >> buckets;

    DBG_Assert( sets == theNumSets , ( << "Error loading flexpoint. Flexpoint created with " << sets << " sets, but simulator configured for " << theNumSets << " sets." ));
    DBG_Assert( buckets == theNumBuckets , ( << "Error loading flexpoint. Flexpoint created with " << buckets << " buckets, but simulator configured for " << theNumBuckets << " buckets." ));

    for (int32_t set = 0; set < theNumSets; set++) {
      for (int32_t bucket = 0; bucket < theNumBuckets; bucket++) {
        StdDirEntrySerializer serializer;
        ia >> serializer;
        theDirectory[set][bucket].theTaglessEntry = serializer;
      }
    }
    return true;
  }

  static AbstractDirectory * createInstance(std::list<std::pair<std::string, std::string> > &args) {
    TaglessDirectory * directory = new TaglessDirectory();

    directory->parseGenericOptions(args);

    std::list<std::pair<std::string, std::string> >::iterator iter = args.begin();
    for (; iter != args.end(); iter++) {
      if (strcasecmp(iter->first.c_str(), "sets") == 0) {
        directory->theNumSets = boost::lexical_cast<int>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "buckets") == 0) {
        directory->theNumBuckets = boost::lexical_cast<int>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "hash") == 0) {
        directory->addHashPolicy(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "xor_shift") == 0) {
        directory->theHashXORShift = boost::lexical_cast<int>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "rotation") == 0) {
        directory->theRotation = boost::lexical_cast<int>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "track_collisions") == 0) {
        directory->theTrackCollisions = boost::lexical_cast<bool>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "track_bit_counts") == 0) {
        directory->theTrackBitCounts = boost::lexical_cast<bool>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "track_pattern_counts") == 0) {
        directory->theTrackBitPatterns = boost::lexical_cast<bool>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "update_on_snoop") == 0) {
        directory->theUpdateOnSnoop = boost::lexical_cast<bool>(iter->second);
      } else if (strcasecmp(iter->first.c_str(), "partitioned") == 0) {
        directory->thePartitioned = boost::lexical_cast<bool>(iter->second);
      } else {
        DBG_Assert(false, ( << "Unknown option passed to TaglessDirectory: '" << iter->first << "'" ));
      }
    }

    return directory;
  }

  static const std::string name;

};

REGISTER_DIRECTORY_TYPE(TaglessDirectory, "Tagless");

}; // namespace
