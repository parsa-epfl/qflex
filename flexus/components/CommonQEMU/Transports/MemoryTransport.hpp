// DO-NOT-REMOVE begin-copyright-block 
//QFlex consists of several software components that are governed by various
//licensing terms, in addition to software that was developed internally.
//Anyone interested in using QFlex needs to fully understand and abide by the
//licenses governing all the software components.
//
//### Software developed externally (not by the QFlex group)
//
//    * [NS-3](https://www.gnu.org/copyleft/gpl.html)
//    * [QEMU](http://wiki.qemu.org/License) 
//    * [SimFlex] (http://parsa.epfl.ch/simflex/)
//
//Software developed internally (by the QFlex group)
//**QFlex License**
//
//QFlex
//Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification,
//are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//    * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//      nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written
//      permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
//EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// DO-NOT-REMOVE end-copyright-block   
#ifndef FLEXUS_TRANSPORTS__MEMORY_TRANSPORT_HPP_INCLUDED
#define FLEXUS_TRANSPORTS__MEMORY_TRANSPORT_HPP_INCLUDED

#include <core/transport.hpp>

#include <components/CommonQEMU/Slices/MemoryMessage.hpp>
#include <components/CommonQEMU/Slices/ExecuteState.hpp>
#include <components/CommonQEMU/Slices/MemOp.hpp>
#include <components/CommonQEMU/Slices/Mux.hpp>
#include <components/CommonQEMU/Slices/TransactionTracker.hpp>
#include <components/CommonQEMU/Slices/DirectoryEntry.hpp>
#include <components/CommonQEMU/Slices/DestinationMessage.hpp>
#include <components/CommonQEMU/Slices/NetworkMessage.hpp>
#include <components/CommonQEMU/Slices/TaglessDirMsg.hpp>

namespace Flexus {
namespace SharedTypes {

#ifndef FLEXUS_TAG_MemoryMessageTag
#define FLEXUS_TAG_MemoryMessageTag
struct MemoryMessageTag_t {};
namespace {
MemoryMessageTag_t MemoryMessageTag;
}
#endif //FLEXUS_TAG_MemoryMessageTag

#ifndef FLEXUS_TAG_ExecuteStateTag
#define FLEXUS_TAG_ExecuteStateTag
struct ExecuteStateTag_t {};
namespace {
ExecuteStateTag_t ExecuteStateTag;
}
#endif //FLEXUS_TAG_ExectueStateTag

#ifndef FLEXUS_TAG_uArchStateTag
#define FLEXUS_TAG_uArchStateTag
struct uArchStateTag_t {};
namespace {
uArchStateTag_t uArchStateTag;
}
#endif //FLEXUS_TAG_uArchStateTag

#ifndef FLEXUS_TAG_MuxTag
#define FLEXUS_TAG_MuxTag
struct MuxTag_t {};
namespace {
MuxTag_t MuxTag;
}
#endif //FLEXUS_TAG_MuxTag

#ifndef FLEXUS_TAG_BusTag
#define FLEXUS_TAG_BusTag
struct BusTag_t {};
namespace {
BusTag_t BusTag;
}
#endif //FLEXUS_TAG_BusTag

#ifndef FLEXUS_TAG_DirectoryEntryTag
#define FLEXUS_TAG_DirectoryEntryTag
struct DirectoryEntryTag_t {};
struct DirectoryEntry;
namespace {
DirectoryEntryTag_t DirectoryEntryTag;
}
#endif //FLEXUS_TAG_DirectoryEntryTag

#ifndef FLEXUS_TAG_TransactionTrackerTag
#define FLEXUS_TAG_TransactionTrackerTag
struct TransactionTrackerTag_t {};
struct TransactionTracker;
namespace {
TransactionTrackerTag_t TransactionTrackerTag;
}
#endif //FLEXUS_TAG_TransactionTrackerTag

#ifndef FLEXUS_TAG_DestinationTag
#define FLEXUS_TAG_DestinationTag
struct DestinationTag_t {};
namespace {
DestinationTag_t DestinationTag;
}
#endif //FLEXUS_TAG_DestinationTag

#ifndef FLEXUS_TAG_NetworkMessageTag
#define FLEXUS_TAG_NetworkMessageTag
struct NetworkMessageTag_t {};
namespace {
NetworkMessageTag_t NetworkMessageTag;
}
#endif //FLEXUS_TAG_NetworkMessageTag

#ifndef FLEXUS_TAG_TaglessDirMsgTag
#define FLEXUS_TAG_TaglessDirMsgTag
struct TaglessDirMsgTag_t {};
namespace {
TaglessDirMsgTag_t TaglessDirMsgTag;
}
#endif //FLEXUS_TAG_TaglessDirMsgTag

typedef Transport
< mpl::vector
< transport_entry< MemoryMessageTag_t, MemoryMessage >
, transport_entry< ExecuteStateTag_t, ExecuteState >
, transport_entry< uArchStateTag_t, MemOp >
, transport_entry< MuxTag_t, Mux >
, transport_entry< BusTag_t, Mux >
, transport_entry< DirectoryEntryTag_t, DirectoryEntry >
, transport_entry< TransactionTrackerTag_t, TransactionTracker >
, transport_entry< DestinationTag_t, DestinationMessage >
, transport_entry< NetworkMessageTag_t, NetworkMessage >
, transport_entry< TaglessDirMsgTag_t, TaglessDirMsg >
>
> MemoryTransport;

} //namespace SharedTypes
} //namespace Flexus

#endif //FLEXUS_TRANSPORTS__MEMORY_TRANSPORT_HPP_INCLUDED

