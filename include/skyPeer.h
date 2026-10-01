/*
 * File Name: skyPeer.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 28/09/2026
 * Purpose: Declares necessary objects for working with peers.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace skyx
{
    struct skyPacket_PeerQueryRequest {
        std::string                             peer_uuid;
        std::string                             file_hash;
        uint64_t                                chunk_size;
    };

    struct fragment_description_t {
        uint64_t                                offset;
    };

    struct skyPacket_PeerQueryResponse {
        std::string                             owner_uuid;
        bool                                    owns_file;

        std::vector<fragment_description_t>     fragments;
    };
}
