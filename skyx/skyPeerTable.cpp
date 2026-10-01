/*
 * File Name: skyPeerTable.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Implements tracker peer registration and lookup.
 */

#include <skyPeerTable.h>
#include <skySerialize.hpp>
#include <algorithm>
#include <string_view>


namespace skyx
{
    bool skyPeerTable::register_peer(const skyPeerInfo &p) {
        const auto iter = std::find_if(m_peers.begin(), m_peers.end(), [p] (skyPeerInfo &c_p) {
            return std::string_view(c_p.peer_uuid) == std::string_view(p.peer_uuid);
        });

        if ( iter == m_peers.end() ) {
            m_peers.push_back(p);
            return true;
        }

        return false;
    }

    std::vector<skyPeerInfo> skyPeerTable::get_peers(const std::function<bool(Peer_UUID)>& predicate) {
        std::vector<skyPeerInfo> peers;
        for (auto &p : m_peers) {
            if ( !predicate(p.peer_uuid) ) {
                peers.emplace_back(p);
            }
        }
        return peers;
    }
}
