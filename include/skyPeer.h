/*
 * File Name: skyPeer.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 28/09/2026
 * Purpose: Declares necessary objects for working with peers.
 */

#pragma once

#include <common.h>
#include <job.h>
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

    struct index_tree_info_t {
        std::string         file_path;
        std::string         file_hash;
        std::string         path_key;
        uint64_t            size;
    };

    struct peer_index_tree_t {
        std::vector<index_tree_info_t>      index;
    };

    struct skyPacket_PeerIndexQueryRequest {
        Peer_UUID       request_owner;
    };

    struct skyPacket_PeerIndexQueryResponse {
        Peer_UUID               index_owner;
        peer_index_tree_t       index;
    };

    // job logic
    struct ActiveJob {
        std::vector<Peer_UUID>      peers;
        std::string                 file_name;
        std::string                 path_key;
        fs_file                     job_src;
        bool                        completed;
        uint64_t                    started_at;

        public:
            std::string             get_file_name_only();
    };
}
