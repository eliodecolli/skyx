#include "common.h"
#include "netUdpSocket.h"
#include "skyPacket.h"
#include "skyTracker.h"
#include <skyClient.h>
#include <format>
#include <algorithm>
#include <skySerialize.hpp>

namespace skyx
{
    template<typename T>
    concept SocketReceiver = requires (T obj, cb<T, const netPacket&> c) {
        obj.on_receive(c);
    };

    template<SocketReceiver TSocket>
    void bootstrap_socket(TSocket *socket, std::function<void(const skyPacket&)> callback)
    {
        socket->on_receive([callback = std::move(callback)] (TSocket*, const netPacket &packet) {
            auto s_packet = deserialize_packet(packet);
            callback(s_packet);
        });
    }

    skyClient::skyClient(uv_loop_t *owner_loop, const Peer_UUID &uuid)
                : m_uuid(uuid), m_udp_socket(owner_loop)
    {
        // I can consolidate these two into a single 'on_sky_packet()' but then I'd have to split them according to some
        // ugly ass switch rules based on their packet type
        // Instead this way we have a clear separation at the expense of DRY :)
        //
        m_loop = owner_loop;
        bootstrap_socket(&m_udp_socket, [&] (const skyPacket &packet)
            {
                on_peer_message(packet);
            });
    }

    void skyClient::register_presence(const endpoint_t &ep)
    {
        std::string tracker_ep { std::format("{}:{}", ep.t_ip, ep.t_port) };
        const auto &it = std::find_if(m_trackers.begin(), m_trackers.end(), [&tracker_ep] (const connectedTracker_t &tracker)
            {
                return tracker_ep == std::string { std::format("{}:{}", tracker.m_endpoint.t_ip, tracker.m_endpoint.t_port) };
            });

        if ( it != m_trackers.end() )
        {
            std::printf("skyClient:: Tracker %s already registered\n", tracker_ep.c_str());
            return;
        }

        skyPacket_TrackerRegister req;
        req.uuid = m_uuid;
        req.attributes.emplace_back(VERSION_ATTR, SKYX_VERSION);

        skyPacket packet;
        packet.type = SkyPacketType::TRACKER_REGISTER;

        serialize_packet_tracker_register(req, packet.buff);
        packet.len = packet.buff.size();

        netPacketBuffer frag;
        serialize_packet(packet, frag);

        netClient tracker_tcp { m_loop };
        tracker_tcp.connect(ep.t_ip, ep.t_port, [&] (netClient *c, bool connected)
            {
                if ( connected )
                {
                    bootstrap_socket(c, [&] (const skyPacket &packet)
                        {
                            on_tracker_response(packet);
                        });
                    c->send(frag);

                    // yeah idk about this
                    // feels strange
                    m_trackers.emplace_back(tracker_tcp, ep);
                    std::printf("skyClient:: Registered tracker %s:%d\n", ep.t_ip.c_str(), ep.t_port);
                }
            });
    }
};
