/*
 * File Name: skyTracker.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 01/10/2026
 * Purpose: Implements tracker request handling and peer discovery.
 */

#include <common.h>
#include <algorithm>
#include <skyTracker.h>
#include <skyPacket.h>
#include <skySerialize.hpp>
#include <utility>
#include <print>

namespace skyx
{
    const std::string get_peer_udp_punch_value(std::string ip, int port)
    {
        return std::format("{}:{}", ip, port);
    }

    static skyPacketResponse handle_tracker_register(skyTracker *tracker, skyPacket_TrackerRegister req, const std::string& peer_ip, int peer_port) {
        skyPeerInfo peer;
        peer.peer_endpoint.t_port = peer_port;
        peer.peer_endpoint.t_ip = peer_ip;
        peer.peer_attributes.insert(peer.peer_attributes.begin(), req.attributes.begin(), req.attributes.end());
        peer.peer_uuid = req.uuid;

        skyPacket_TrackerRegisterResult result;

        auto added = tracker->register_peer(peer);
        if ( !added ) {
            std::printf("skyTracker:: Peer %s is already present in the system\n", req.uuid.c_str());
            result.ok = false;
            result.message = std::format("Peer {} already registered.", req.uuid);
        }
        else {
            std::printf("skyTracker:: Added peer %s from %s:%d\n", req.uuid.c_str(), peer_ip.c_str(), peer_port);
            result.ok = true;
        }

        // return something maybe?
        skyPacketResponse resp;
        resp.success = true;
        serialize_packet_tracker_register_result(result, resp.message);

        return resp;
    }

    static skyPacketResponse handle_tracker_fetch_peers(skyTracker *tracker, const skyPacket_TrackerFetchPeers& req, const std::string& peer_ip, int peer_port) {
        auto peers = tracker->fetch_peers(req.uuid);
        skyPacketResponse response;

        skyPacket_TrackerFetchPeersResult result { std::move(peers) };
        serialize_packet_tracker_fetch_peers_result(result, response.message);
        response.success = true;

        std::println("skyTracker:: Peer {} ({}) requested friend list: {} total peers.", req.uuid.c_str(), peer_ip.c_str(), result.peers.size());

        return response;
    }

    skyPacketResponse skyTracker::handle_udp_punch_request(const skyPacket &req)
    {
        // extract it
        const auto request = deserialize_packet_tracker_udp_punch_register(req.buff);

        // first make sure its a known peer
        auto &peers = m_peers.get_all();
        auto it = std::find_if(peers.begin(), peers.end(), [&request] (const skyPeerInfo &x)
            {
                return x.peer_uuid == request.uuid;
            });

        skyPacket_UdpPunchRegisterResult result;

        if ( it == peers.end() )
        {
            // nope not found
            result.ok = false;
            result.message = std::format("Unknown peer with uuid {}", request.uuid);
        }
        else
        {
            std::printf(
                "skyTracker:: Updating UDP punch data for peer %s -> %s:%d\n",
                it->peer_uuid.c_str(),
                req.ip.c_str(),
                req.port
            );
            m_peers_udp_punches.insert_or_assign(it->peer_uuid, endpoint_t { req.ip, req.port });

            result.ok = true;
        }

        skyPacketResponse resp;
        serialize_packet_tracker_udp_punch_register_result(result, resp.message);

        return resp;
    }

    void skyTracker::start() {
        std::println("skyTracker:: Starting tracker server");
        server.on_recieve([this] (netServer *m_server, const netPacket &m_packet) {
            this->listen(m_server, m_packet);
        });

        server.listen(8080);
    }

    void skyTracker::listen(netServer *p_server, const netPacket& packet) {
        skyPacket incoming = deserialize_packet(packet.buffer);
        skyPacketResponse result;

        std::println("skyTracker:: Received packet type='{}'", std::to_underlying(incoming.type));

        switch (incoming.type) {
            case SkyPacketType::TRACKER_REGISTER:
                {
                    auto tracker_register = deserialize_packet_tracker_register(incoming.buff);
                    result = handle_tracker_register(this, tracker_register, packet.address, packet.port);
                    break;
                }

            case SkyPacketType::TRACKER_FETCH_PEERS:
                {
                    auto fetch_request = deserialize_packet_tracker_fetch_peers(incoming.buff);
                    result = handle_tracker_fetch_peers(this, fetch_request, packet.address, packet.port);
                    break;
                }

            default:
                {
                    std::printf("skyTracker:: SkyPacketType not handled\n");
                    break;
                }
        }

        // TODO: ALWAYS SEND SOMETHING BACK FFS! Let them know if there was an error or something.
        if ( result.success ) {
            auto msg_size = result.message.size();

            // sometimes we don't really wanna send anything back
            if ( msg_size > 0 ) {
                auto resp = packet.make_response(std::move(result.message), msg_size);
                p_server->send_data(resp);
            }
        }
    }

    bool skyTracker::register_peer(const skyPeerInfo &peer) {
        return m_peers.register_peer(peer);
    }

    std::vector<skyPeerInfo> skyTracker::fetch_peers(const Peer_UUID& exclude) {
        const std::function pred = [exclude] (const Peer_UUID &peer)
        {
            return peer == exclude;
        };
        return m_peers.get_peers(pred);
    }
}
