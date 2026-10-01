/*
 * File Name: test_serializer.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Tests skyX protocol serialization and deserialization.
 */

#include <netPacket.h>
#include <skyPeer.h>
#include <skySerialize.hpp>
#include <string_view>
#include <format>
#include <cstdio>
#include <cstdlib>

using namespace skyx;

#ifdef SKYX_ASSERT
#undef SKYX_ASSERT
#endif

#define SKYX_ASSERT(expr, msg) \
if (!expr) { \
     std::fprintf(stderr, "\033[31m Assert Error:\033[0m %s\n", msg); \
    std::abort(); \
} \

void test_serialization() {
    skyPacket p;
    p.type = SkyPacketType::TRACKER_REGISTER;
    p.len = 5;
    p.buff.emplace_back('H');
    p.buff.emplace_back('e');
    p.buff.emplace_back('l');
    p.buff.emplace_back('l');
    p.buff.emplace_back('o');

    netPacketBuffer serialized;
    serialize_packet(p, serialized);
    auto deserialized = deserialize_packet(serialized);

    SKYX_ASSERT((deserialized.type == SkyPacketType::TRACKER_REGISTER), "Invalid packet type!");
    SKYX_ASSERT((deserialized.len == 5), std::format("Invalid packet length: {}", deserialized.len).c_str());

    std::string s((char*)deserialized.buff.data(), deserialized.len);
    SKYX_ASSERT((std::string_view(s) == std::string_view("Hello")), "Invalid packet buffer!");
}

void test_packet_tracker_register_serialization() {
    skyPacket_TrackerRegister temp;
    temp.uuid = "9c3a37e2-2543-4b9e-8028-a03fbd0b1230";
    temp.attributes.emplace_back("test_attr", "test_attr_val");

    netPacketBuffer buf;
    serialize_packet_tracker_register(temp, buf);
    SKYX_ASSERT((buf.size() > 0), "test_packet_tracker_register_serialization: Serialization failed!");

    auto deserialized = deserialize_packet_tracker_register(buf);

    std::fprintf(stderr, "UUID: '%s'\n", deserialized.uuid.c_str());
    SKYX_ASSERT((deserialized.uuid == temp.uuid), "test_packet_tracker_register_serialization: Invalid UUID!");

    auto attr_name_one = deserialized.attributes.at(0).attr_name;
    auto attr_name_two = temp.attributes.at(0).attr_name;
    SKYX_ASSERT((attr_name_one == attr_name_two), "test_packet_tracker_register_serialization: Invalid attributes!");
}

void test_peer_query_packets_serialize()
{
   {
       skyPacket_PeerQueryRequest req;
       req.peer_uuid = "hi-my-uuid";
       req.chunk_size = 10;
       req.file_hash = "fancy-hash";

       netPacketBuffer buf;
       serialize_packet_peer_query_request(req, buf);

       SKYX_ASSERT((buf.size() > 0), "test_peer_query_packets_serialize: Invalid buffer size after serialization!");

       skyPacket_PeerQueryRequest d_req = deserialize_packet_peer_query_request(buf);
       SKYX_ASSERT((d_req.peer_uuid == req.peer_uuid), "test_peer_query_packets_serialize: Invalid peer uuid deserialization.");
       SKYX_ASSERT((d_req.chunk_size == req.chunk_size), "test_peer_query_packets_serialize: Invalid chuck size deserialization.");
       SKYX_ASSERT((d_req.file_hash == req.file_hash), "test_peer_query_packets_serialize: Invalid file hash deserialization.");

   }

    // response
    {
        skyPacket_PeerQueryResponse resp;
        resp.owner_uuid = "owner-UUID";
        resp.owns_file = true;
        resp.fragments.emplace_back(10);
        resp.fragments.emplace_back(63);

        netPacketBuffer buf;
        serialize_packet_peer_query_response(resp, buf);
        SKYX_ASSERT((buf.size() > 0), "test_peer_query_packets_serialize: Invalid response buffer size! It's ZERO :(");

        skyPacket_PeerQueryResponse d_resp = deserialize_packet_peer_query_response(buf);
        SKYX_ASSERT((d_resp.owner_uuid == resp.owner_uuid), "test_peer_query_packets_serialize: Invalid owner_uuid in response.");
        SKYX_ASSERT((d_resp.owns_file == resp.owns_file), "test_peer_query_packets_serialize: Invalid owns file flag.");

        auto f_zero = d_resp.fragments[0].offset;
        auto f_zero_t = resp.fragments[0].offset;
        SKYX_ASSERT((f_zero == f_zero_t),
            std::format("test_peer_query_packets_serialize: Invalid fragment at index 0 -> {} != {}.", f_zero, f_zero_t).c_str());

        auto f_one = d_resp.fragments[1].offset;
        auto f_one_t = resp.fragments[1].offset;
        SKYX_ASSERT((f_one == f_one_t),
            std::format("test_peer_query_packets_serialize: Invalid fragment at index 1 -> {} != {}.", f_one, f_one_t).c_str());
    }
}

void test_peer_query_dont_throw_on_missing_fragments()
{
    skyPacket_PeerQueryResponse resp;
    resp.owner_uuid = "owner-UUID";
    resp.owns_file = false;

    netPacketBuffer buf;
    serialize_packet_peer_query_response(resp, buf);
    SKYX_ASSERT((buf.size() > 0), "test_peer_query_dont_throw_on_missing_fragments: Invalid response buffer size! It's ZERO :(");

    skyPacket_PeerQueryResponse d_resp = deserialize_packet_peer_query_response(buf);
    SKYX_ASSERT((d_resp.fragments.size() == 0), "test_peer_query_dont_throw_on_missing_fragments: Yeah, this is broken.");
}

int main() {
    test_serialization();
    test_packet_tracker_register_serialization();
    test_peer_query_packets_serialize();
    test_peer_query_dont_throw_on_missing_fragments();
    printf("\033[32mTests passed successfully.\033[0m\n");
    return 0;
}
