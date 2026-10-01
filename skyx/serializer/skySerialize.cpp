/*
 * File Name: skySerialize.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 28/09/2026
 * Purpose: Implements binary serialization for skyX protocol messages.
 */

#include "skyPeer.h"
#include "skyTracker.h"
#include <skySerialize.hpp>

namespace skyx
{
    skyPacket make_packet(SkyPacketType type, const netPacketBuffer& buf) {
        skyPacket packet;
        packet.type = (SkyPacketType) type;
        packet.len = static_cast<uint32_t>(buf.size());
        if ( !buf.empty() ) {
            BinaryWriter writer(packet.buff);
            writer << static_cast<uint32_t>(buf.size());
            packet.buff.insert(packet.buff.end(), buf.begin(), buf.end());
        }

        return packet;
    }

    // actual implementation
    netPacketBuffer serialize_packet(const skyPacket &packet, netPacketBuffer &buf) {
        BinaryWriter writer(buf);

        // the header uint8_t
        uint8_t p_type = static_cast<uint8_t>(packet.type);
        writer << p_type;

        // the length of the buffer uint32_t
        writer << packet.len;

        // the buffer
        writer << packet.buff;

        return writer.get_buffer();
    }

    const skyPacket deserialize_packet(const netPacketBuffer &data) {
        skyPacket packet;
        BinaryReader reader(data);

        // first read the first byte -> packet type
        reader >> packet.type;

        // now read the next 4 bytes -> buf len
        reader >> packet.len;

        // now fill in the buffer
        reader.read_buffer(packet.buff, packet.len);

        return packet;
    }


    void serialize_packet_tracker_udp_punch_register(const skyPacket_UdpPunchRegister &packet, netPacketBuffer &buf)
    {
        BinaryWriter writer { buf };
        writer << static_cast<uint32_t>(packet.uuid.size());
        writer << packet.uuid;
    }

    skyPacket_UdpPunchRegister deserialize_packet_tracker_udp_punch_register(const netPacketBuffer &buf)
    {
        BinaryReader reader { buf };
        skyPacket_UdpPunchRegister result;
        uint32_t len;
        reader >> len;
        reader.read_string(&result.uuid, len);
        return result;
    }

    void serialize_packet_tracker_udp_punch_register_result(const skyPacket_UdpPunchRegisterResult &packet, netPacketBuffer &buf)
    {
        BinaryWriter writer { buf };
        writer << packet.ok;
        if ( packet.message.size() > 0 )
        {
            writer << static_cast<uint32_t>(packet.message.size());
            writer << packet.message;
        }
    }

    skyPacket_UdpPunchRegisterResult deserialize_packet_tracker_udp_punch_result(const netPacketBuffer &buf)
    {
        BinaryReader reader { buf };
        skyPacket_UdpPunchRegisterResult result;

        reader >> result.ok;
        if ( !reader.eof() )
        {
            uint32_t len;
            reader >> len;
            reader.read_string(&result.message, len);
        }

        return result;
    }

    void serialize_packet_tracker_register(const skyPacket_TrackerRegister &packet, netPacketBuffer &buf) {
        BinaryWriter writer(buf);

        // pack peer's uuid
        // len
        auto uuid_len = static_cast<uint32_t>(packet.uuid.size());
        writer << uuid_len;

        // uuid
        writer << packet.uuid;

        // pack peer's attributes
        // count
        auto attr_count = static_cast<uint32_t>(packet.attributes.size());
        writer << attr_count;

        // attributes
        for ( auto &attr : packet.attributes ) {
            // attr name
            auto name_len = static_cast<uint32_t>(attr.attr_name.size());
            writer << name_len;
            writer << attr.attr_name;

            // attr value
            auto val_len = static_cast<uint32_t>(attr.attr_val.size());
            writer << val_len;
            writer << attr.attr_val;
        }
    }

    skyPacket_TrackerRegister deserialize_packet_tracker_register(const netPacketBuffer &buf) {
        skyPacket_TrackerRegister packet;

        BinaryReader reader(buf);

        // peer uuid first
        // len
        uint32_t uuid_len;
        reader >> uuid_len;

        // uuid
        reader.read_string(&packet.uuid, uuid_len);

        // peer attributes
        // attributes count
        uint32_t attr_count;
        reader >> attr_count;

        // now each attribute
        for ( size_t i = 0; i < attr_count; i++ ) {
            // name len
            uint32_t name_len;
            reader >> name_len;

            // name
            std::string s;
            reader.read_string(&s, name_len);

            // val len
            uint32_t val_len;
            reader >> val_len;

            // val
            std::string v;
            reader.read_string(&v, val_len);

            // add attribute
            packet.attributes.emplace_back(s, v);
        }

        return packet;
    }

    void serialize_packet_tracker_register_result(const skyPacket_TrackerRegisterResult& packet, netPacketBuffer &buf)
    {
        BinaryWriter writer(buf);
        writer << packet.ok;

        if ( !packet.ok )
        {
            writer << static_cast<uint32_t>(packet.message.size());
            writer << packet.message;
        }
    }

    skyPacket_TrackerRegisterResult deserialize_packet_tracker_result(const netPacketBuffer& buf)
    {
        skyPacket_TrackerRegisterResult packet;
        BinaryReader reader(buf);
        reader >> packet.ok;

        if ( !packet.ok )
        {
            uint32_t msg_len;
            reader >> msg_len;
            reader.read_string(&packet.message, msg_len);
        }
        return packet;
    }

    void serialize_packet_tracker_fetch_peers(const skyPacket_TrackerFetchPeers& packet, netPacketBuffer& buf) {
        BinaryWriter writer(buf);
        auto uuid_len = static_cast<uint32_t>(packet.uuid.size());
        writer << uuid_len;
        writer << packet.uuid;
    }

    skyPacket_TrackerFetchPeers deserialize_packet_tracker_fetch_peers(const netPacketBuffer& buf) {
        skyPacket_TrackerFetchPeers packet;
        BinaryReader reader(buf);
        uint32_t uuid_len;
        reader >> uuid_len;
        reader.read_string(&packet.uuid, uuid_len);

        return packet;
    }

    void serialize_packet_tracker_fetch_peers_result(const skyPacket_TrackerFetchPeersResult& packet,
        netPacketBuffer& buf) {
        BinaryWriter writer(buf);
        auto peer_count = static_cast<uint32_t>(packet.peers.size());
        writer << peer_count;
        for (auto &p : packet.peers) {
            serialize_peer_info(p, buf);
        }
    }

    skyPacket_TrackerFetchPeersResult deserialize_packet_tracker_fetch_peers_result(const netPacketBuffer& buf) {
        skyPacket_TrackerFetchPeersResult packet;
        BinaryReader reader(buf);
        uint32_t peer_count;
        reader >> peer_count;

        for (size_t i = 0; i < peer_count; i++) {
            auto peer_info = deserialize_peer_info(reader);
            packet.peers.push_back(peer_info);
        }
        return packet;
    }

    void serialize_peer_info(const skyPeerInfo& peer, netPacketBuffer& buf) {
        BinaryWriter writer(buf);
        auto uuid_len = static_cast<uint32_t>(peer.peer_uuid.size());
        writer << uuid_len;
        writer << peer.peer_uuid;

        auto ip_len = static_cast<uint32_t>(peer.peer_endpoint.t_ip.size());
        writer << ip_len;
        writer << peer.peer_endpoint.t_ip;

        writer << peer.peer_endpoint.t_port;

        auto attr_count = static_cast<uint32_t>(peer.peer_attributes.size());
        writer << attr_count;
        for (auto &attr : peer.peer_attributes) {
            auto name_len = static_cast<uint32_t>(attr.attr_name.size());
            writer << name_len;
            writer << attr.attr_name;

            auto val_len = static_cast<uint32_t>(attr.attr_val.size());
            writer << val_len;
            writer << attr.attr_val;
        }
    }

    skyPeerInfo deserialize_peer_info(BinaryReader& reader) {
        skyPeerInfo p;
        uint32_t uuid_len;
        reader >> uuid_len;
        reader.read_string(&p.peer_uuid, uuid_len);

        uint32_t ip_len;
        reader >> ip_len;
        reader.read_string(&p.peer_endpoint.t_ip, ip_len);

        reader >> p.peer_endpoint.t_port;

        uint32_t attr_count;
        reader >> attr_count;
        for (size_t i = 0; i < attr_count; i++) {
            uint32_t attr_name_len;
            reader >> attr_name_len;
            std::string attr_name;
            reader.read_string(&attr_name, attr_name_len);

            uint32_t attr_val_len;
            reader >> attr_val_len;
            std::string attr_val;
            reader.read_string(&attr_val, attr_val_len);

            p.peer_attributes.emplace_back(attr_name, attr_val);
        }

        return p;
    }

    void serialize_packet_peer_query_request(const skyPacket_PeerQueryRequest &packet, netPacketBuffer &buf)
    {
        BinaryWriter writer{buf};
        writer << static_cast<uint64_t>(packet.peer_uuid.size());
        writer << packet.peer_uuid;
        writer << packet.chunk_size;
        writer << static_cast<uint64_t>(packet.file_hash.size());
        writer << packet.file_hash;
    }

    skyPacket_PeerQueryRequest deserialize_packet_peer_query_request(const netPacketBuffer &buf)
    {
        BinaryReader reader{buf};
        skyPacket_PeerQueryRequest retval;

        uint64_t uuid_len;
        reader >> uuid_len;
        reader.read_string(&retval.peer_uuid, uuid_len);

        reader >> retval.chunk_size;

        uint64_t hash_len;
        reader >> hash_len;
        reader.read_string(&retval.file_hash, hash_len);

        return retval;
    }

    void serialize_packet_peer_query_response(const skyPacket_PeerQueryResponse &packet, netPacketBuffer &buf)
    {
        BinaryWriter writer{buf};
        writer << static_cast<uint64_t>(packet.owner_uuid.size());
        writer << packet.owner_uuid;
        writer << packet.owns_file;

        writer << static_cast<uint64_t>(packet.fragments.size());
        for ( auto &fragment : packet.fragments )
        {
            writer << fragment.offset;
        }
    }

    skyPacket_PeerQueryResponse deserialize_packet_peer_query_response(const netPacketBuffer &buf)
    {
        BinaryReader reader{buf};
        skyPacket_PeerQueryResponse retval;

        uint64_t uuid_len;
        reader >> uuid_len;
        reader.read_string(&retval.owner_uuid, uuid_len);

        reader >> retval.owns_file;

        if ( !reader.eof() )
        {
            uint64_t num_fragments;
            reader >> num_fragments;

            for ( uint64_t i = 0; i < num_fragments; i++ )
            {
                fragment_description_t f;
                reader >> f.offset;
                retval.fragments.emplace_back(f);
            }
        }

        return retval;
    }
}
