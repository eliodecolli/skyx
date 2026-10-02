/*
 * File Name: skyClient.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Declares the SkyX peer client interface.
 */

#pragma once

#include "common.h"
#include "uv.h"
#include <skyPeer.h>
#include <netClient.h>
#include <netUdpSocket.h>
#include <skyPacket.h>


namespace skyx
{
    inline const std::string VERSION_ATTR = "skyx-ver";

    class skyClient {

        struct connectedPeer_t {
            Peer_UUID       uuid;
            std::string     ip;
            int             port;
        };

        struct connectedTracker_t {
            netClient   m_tcp_socket;
            endpoint_t  m_endpoint;
        };

        private:
            std::unordered_map<netClientName, connectedPeer_t>      m_peers;
            std::unordered_map<std::string, ActiveJob>              m_jobs;
            std::vector<connectedTracker_t>                         m_trackers;

        private:
            Peer_UUID                                               m_uuid;
            uv_loop_t                                               *m_loop;
            netUdpSocket                                            m_udp_socket;

        private:
            void on_tracker_response(const skyPacket packet);
            void on_peer_message(const skyPacket &packet);

        public:
            void register_presence(const endpoint_t &addr);
            void broadcast_message(const skyPacket &packet);

        public:
            skyClient(uv_loop_t *owner_loop, const Peer_UUID &uuid);
            ~skyClient() = default;
    };
}
