/*
 * File Name: skyTracker.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 01/10/2026
 * Purpose: Declares the tracker service and its request payloads.
 */

#pragma once

#include <netServer.h>
#include <skyPacket.h>
#include <skyPeerTable.h>
#include <common.h>
#include <unordered_map>
#include <vector>
#include <netUdpSocket.h>

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

    struct skyPacket_UdpPunchRegister {
        Peer_UUID   uuid;
    };

    struct skyPacket_UdpPunchRegisterResult {
        bool ok;
        std::string message;
    };

    class skyTracker {
    private:
        netServer                                   server;
        netUdpSocket                                m_udp;
        skyPeerTable                                m_peers;
        std::unordered_map<Peer_UUID, endpoint_t>   m_peers_udp_punches;

    private:
        void                    listen(netServer *p_server, const netPacket& incoming);
        skyPacketResponse       handle_udp_punch_request(const skyPacket &req);

    public:
        void start();
        skyTracker() : server{}, m_udp{server.get_loop()} {}

        bool register_peer(const skyPeerInfo &peer);
        std::vector<skyPeerInfo> fetch_peers(const Peer_UUID& exclude);
    };
}
