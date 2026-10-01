/*
 * File Name: netPacket.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Defines network packet payload and endpoint types.
 */

#pragma once

#include <string>
#include <vector>

namespace skyx
{
    using netPacketBuffer =
        std::vector<uint8_t>;

    using netClientName = std::string;

    struct netPacket {
        std::string address;
        int port;

        netPacketBuffer buffer;
        size_t buffer_size;

        inline netPacket make_response(netPacketBuffer &&buf, const size_t size) const {
            netPacket resp;
            resp.address = this->address;
            resp.port = this->port;

            resp.buffer = buf;
            resp.buffer_size = size;

            return resp;
        }
    };

}
