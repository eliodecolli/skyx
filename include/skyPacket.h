/*
 * File Name: skyPacket.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 28/09/2026
 * Purpose: Defines skyX protocol packet structures and types.
 */

#pragma once
#include "netPacket.h"

namespace skyx
{

    enum class SkyPacketType : uint8_t {
        // tracker
        TRACKER_REGISTER,
        TRACKER_REGISTER_RESULT,
        TRACKER_DELIST,
        TRACKER_FETCH_PEERS,
        TRACKER_FETCH_PEERS_RESULT,

        // peer
        PEER_QUERY,
        PEER_QUERY_RESULT,
        PEER_FRAGMENT_REQUEST,
        PEER_FRAGMENT,
    };


    struct skyPacket {
        SkyPacketType   type;
        uint32_t        len;
        netPacketBuffer buff;
    };


    struct skyPacketResponse {
        bool                success;
        netPacketBuffer     message;
    };

}
