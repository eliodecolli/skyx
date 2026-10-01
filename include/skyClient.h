/*
 * File Name: skyClient.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Declares the skyX peer client interface.
 */

#pragma once

#include "skyPeer.h"
#include "skyTracker.h"
#include <skyPacket.h>
#include <common.h>
#include <unordered_map>


namespace skyx
{
    class skyClient {

        struct connectedPeer_t {
            Peer_UUID       uuid;
            std::string     ip;
            int             port;
        };

    private:
        std::unordered_map<netClientName, connectedPeer_t>      m_peers;

    private:
        void on_tracker_register_result(const skyPacket_TrackerRegisterResult packet);

        void on_peer_query_request(const skyPacket_PeerQueryRequest request);
        void on_peer_query_response(const skyPacket_PeerQueryResponse response);

    public:
        void register_presence(const endpoint_t &addr);
        void broadcast_message(const skyPacket &packet);
    };
}
