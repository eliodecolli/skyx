/*
 * File Name: skyPeerTable.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Declares the tracker peer registration table.
 */

#pragma once

#include <vector>
#include <functional>
#include "common.h"

namespace skyx
{
    class skyPeerTable {
    private:
        std::vector<skyPeerInfo> m_peers;

        struct m_table {
            std::vector<skyPeerInfo> peers;
        };

    public:
        bool register_peer(const skyPeerInfo &p);
        std::vector<skyPeerInfo>& get_all();
        std::vector<skyPeerInfo> get_peers(const std::function<bool(Peer_UUID)>& predicate);
    };
}
