/*
 * File Name: skyTracker.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Declares the tracker service and its request payloads.
 */

#pragma once

#include "netServer.h"
#include "skyPeerTable.h"
#include "common.h"
#include <vector>

namespace skyx
{
    struct skyPacket_TrackerRegister {
        Peer_UUID                           uuid;
        std::vector<skyPeerAttribute>       attributes;
    };

    struct skyPacket_TrackerRegisterResult {
        std::string     message;
        bool            ok;
    };

    struct skyPacket_TrackerFetchPeers {
        Peer_UUID   uuid;
    };

    struct skyPacket_TrackerFetchPeersResult {
        std::vector<skyPeerInfo> peers;
    };

    class skyTracker {
    private:
        netServer server;
        skyPeerTable m_peers;

    private:
        void listen(netServer *p_server, const netPacket& incoming);

    public:
        void start();
        skyTracker() : server{} {}

        bool register_peer(const skyPeerInfo &peer);
        std::vector<skyPeerInfo> fetch_peers(const Peer_UUID& exclude);
    };
}
