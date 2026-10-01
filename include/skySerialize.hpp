/*
 * File Name: skySerialize.hpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 01/10/2026
 * Purpose: Declares binary serialization for skyX protocol messages.
 */

#include "netPacket.h"
#include "skyPacket.h"
#include "skyPeer.h"
#include "skyTracker.h"
#include <bit>
#include <iterator>
#include <type_traits>
#include <cstring>

namespace skyx
{
    #define SKYX_DEFAULT_PACKET_BUFFER_SIZE 1024

    class BinaryWriter
    {
    private:
        netPacketBuffer &m_buf;

    private:
        template<typename T>
        void pack_member(netPacketBuffer *dest, T &val, size_t size) {
            static_assert(std::is_trivially_copyable_v<T>);

            auto val_ptr = reinterpret_cast<const uint8_t*>(&val);
            auto val_ptr_len = size;

            std::vector<uint8_t> temp;

            for ( size_t i = 0; i < val_ptr_len; i++ ) {
                auto cp = *(val_ptr + i);
                temp.emplace_back(cp);
            }

            // always default to big endian
            if constexpr (std::endian::native == std::endian::little)
            {
                // swap to the actual thing
                dest->insert(dest->end(), temp.crbegin(), temp.crend());
            }
            else
            {
                dest->insert(dest->end(), temp.cbegin(), temp.cend());
            }
        }

        void pack_string(netPacketBuffer *dest, const std::string &str) {
            dest->insert(dest->end(), str.cbegin(), str.cend());
        }

    public:
        explicit BinaryWriter(netPacketBuffer &buf) : m_buf(buf) {
            m_buf.reserve(SKYX_DEFAULT_PACKET_BUFFER_SIZE);  // reserve some space to avoid unnecessary memory copy
        }
        ~BinaryWriter() = default;

    public:

        template<typename T>
        void operator << (const T &val)
        {
            this->pack_member(&m_buf, val, sizeof(val));
        }

        void operator << (const std::string &val)
        {
            this->pack_string(&m_buf, val);
        }

        void operator << (const netPacketBuffer &buf)
        {
            // we dont check for endianess on already serialized buffers
            // these cases usually contain serialized structs
            m_buf.insert(m_buf.end(), buf.begin(), buf.end());
        }

        netPacketBuffer get_buffer()
        {
            return m_buf;
        }
    };

    class BinaryReader
    {
    private:
        const netPacketBuffer &m_buf;
        netPacketBuffer::const_iterator m_iter;

    private:
        void pop_str(std::string *dest, uint32_t size) {
            dest->resize(size);
            std::memcpy(dest->data(), std::to_address(m_iter), size);
            m_iter += size;
        }

        template<typename T>
        void pop_member(T *dest, uint32_t size) {
            static_assert(std::is_trivially_copyable_v<T>);

            // iter is the current position, use it and update it
            if constexpr (std::endian::native == std::endian::little)
            {
                auto begin = std::make_reverse_iterator(m_iter + size);
                auto end = std::make_reverse_iterator(m_iter);

                std::copy(begin, end, reinterpret_cast<uint8_t*>(dest));
            }
            else
            {
                std::memcpy(dest, std::to_address(m_iter), size);
            }
            m_iter += size;
        }

    public:
        explicit BinaryReader(const netPacketBuffer &buf) : m_buf(buf)
        {
            m_iter = m_buf.cbegin();
        }

        ~BinaryReader() = default;

    public:
        template<typename T>
        void operator >> (T &val)
        {
            this->pop_member(&val, sizeof(T));
        }

        void read_string(std::string *dest, uint32_t size)
        {
            this->pop_str(dest, size);
        }

        template<typename T>
        void read(T *dest, uint32_t size)
        {
            this->pop_member(dest, size);
        }

        void read_buffer(netPacketBuffer &dest, size_t len)
        {
            for (size_t i = 0; i < len; i++) {
                uint8_t cur;
                this->pop_member(&cur, sizeof(uint8_t));
                dest.emplace_back(cur);
            }
        }

        bool eof()
        {
            return m_iter == m_buf.cend();
        }
    };

    // generalized packet stuff
    skyPacket make_packet(SkyPacketType type, const netPacketBuffer &buf);

    netPacketBuffer serialize_packet(const skyPacket &packet, netPacketBuffer &buf);
    const skyPacket deserialize_packet(const netPacketBuffer &data);

    // tracker register
    void serialize_packet_tracker_register(const skyPacket_TrackerRegister &packet, netPacketBuffer &buf);
    skyPacket_TrackerRegister deserialize_packet_tracker_register(const netPacketBuffer& buf);

    void serialize_packet_tracker_register_result(const skyPacket_TrackerRegisterResult &packet, netPacketBuffer &buf);
    skyPacket_TrackerRegisterResult deserialize_packet_tracker_result(const netPacketBuffer& buf);

    // tracker fetch peers
    void serialize_packet_tracker_fetch_peers(const skyPacket_TrackerFetchPeers &packet, netPacketBuffer &buf);
    skyPacket_TrackerFetchPeers deserialize_packet_tracker_fetch_peers(const netPacketBuffer &buf);

    void serialize_packet_tracker_fetch_peers_result(const skyPacket_TrackerFetchPeersResult &packet, netPacketBuffer &buf);
    skyPacket_TrackerFetchPeersResult deserialize_packet_tracker_fetch_peers_result(const netPacketBuffer &buf);

    void serialize_peer_info(const skyPeerInfo &peer, netPacketBuffer &buf);
    skyPeerInfo deserialize_peer_info(BinaryReader &reader);

    // tracker udp hole punching
    void serialize_packet_tracker_udp_punch_register(const skyPacket_UdpPunchRegister &packet, netPacketBuffer &buf);
    skyPacket_UdpPunchRegister deserialize_packet_tracker_udp_punch_register(const netPacketBuffer &buf);

    void serialize_packet_tracker_udp_punch_register_result(const skyPacket_UdpPunchRegisterResult &packet, netPacketBuffer &buf);
    skyPacket_UdpPunchRegisterResult deserialize_packet_tracker_udp_punch_result(const netPacketBuffer &buf);

    // peer query
    void serialize_packet_peer_query_request(const skyPacket_PeerQueryRequest &packet, netPacketBuffer &buf);
    skyPacket_PeerQueryRequest deserialize_packet_peer_query_request(const netPacketBuffer &buf);

    void serialize_packet_peer_query_response(const skyPacket_PeerQueryResponse &packet, netPacketBuffer &buf);
    skyPacket_PeerQueryResponse deserialize_packet_peer_query_response(const netPacketBuffer &buf);

    void serialize_fragment_description(const fragment_description_t &desc, netPacketBuffer &buf);
    fragment_description_t deserialize_fragment_description(const netPacketBuffer &buf);
}
