/*
 * File Name: common.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Defines shared peer and tracker data types.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>

#define         SKYX_VERSION    "0.1a"

typedef std::string     Peer_UUID;

template<typename TOwner, typename TPacket>
using cb = std::function<void(TOwner*, const TPacket)>;

struct endpoint_t {
    std::string     t_ip;
    int             t_port;

    endpoint_t() = default;
    endpoint_t(std::string ip, int port) : t_ip{ip}, t_port{port} {}
};

struct skyPeerAttribute {
    std::string     attr_name;
    std::string     attr_val;

    skyPeerAttribute(std::string n, std::string v) : attr_name {n}, attr_val {v} {}
};

struct skyPeerInfo {
    endpoint_t                      peer_endpoint;
    Peer_UUID                       peer_uuid;

    std::vector<skyPeerAttribute>   peer_attributes;
};
