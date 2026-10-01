#include "uv.h"
#include <cstring>
#include <netUdpSocket.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>

namespace skyx
{
    netUdpSocket::netUdpSocket(uv_loop_t *loop)
    {
        m_loop = loop;
        uv_udp_init(m_loop, &m_socket);
        udp_socket_context_t *context = (udp_socket_context_t *) malloc(sizeof(udp_socket_context_t));
        context->owner = this;
        m_socket.data = context;
    }

    void netUdpSocket::bind(int port)
    {
        sockaddr_in addr;
        uv_ip4_addr("0.0.0.0", port, &addr);
        uv_udp_bind(&m_socket, reinterpret_cast<sockaddr*>(&addr), 0);
        m_port = port;
    }

    void netUdpSocket::on_buffer_allocation(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
    {
        buf->base = (char*) malloc(suggested_size);
        buf->len = suggested_size;
    }

    void netUdpSocket::on_udp_receive(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
    {
        if ( nread < 0 )
        {
            std::printf("netUdpSocket:: Error receiving data from UDP socket.\n");
            uv_udp_recv_stop(handle);
        }
        else if ( nread > 0 )
        {
            const sockaddr_in *ep = reinterpret_cast<const sockaddr_in*>(addr);

            char peer_ip[UV_IF_NAMESIZE] = {0};
            uv_ip4_name(ep, peer_ip, sizeof(peer_ip));

            int peer_port = ntohs(ep->sin_port);

            std::printf("netUdpSocket:: Received %lu bytes from %s:%d\n",
                nread, peer_ip, peer_port);

            auto context = reinterpret_cast<udp_socket_context_t*>(handle->data);
            if ( context->owner->m_callback != nullptr )
            {
                // construct the packet now
                netPacket packet;
                packet.address = std::string(peer_ip);
                packet.port = peer_port;
                packet.buffer_size = static_cast<size_t>(nread);

                for (ssize_t i = 0; i < nread; i++ )
                {
                    uint8_t cur = *(reinterpret_cast<uint8_t*>(buf->base + i));
                    packet.buffer.emplace_back(cur);
                }

                context->owner->m_callback(context->owner, packet);
            }
            else
            {
                std::printf("netUdpSocket:: Socket is receiving data but callback is not set, so the data is being ignored.\n");
            }
        }

        free(buf->base);
    }

    void netUdpSocket::on_receive(callback_fn callback)
    {
        m_callback = std::move(callback);
    }

    void netUdpSocket::start()
    {
        // should we allow to start with no callback set?
        //

        if ( m_port == -1 )
        {
            throw std::runtime_error("netUdpSocket:: Cannot start listening on a socket before binding it.");
        }
        uv_udp_recv_start(&m_socket, on_buffer_allocation, on_udp_receive);

        std::printf("netUdpSocket:: Socket listening on port %d\n", m_port);
    }

    void netUdpSocket::on_send_callback(uv_udp_send_t* req, int status)
    {
        // do we need to use futures here and make this operation synchronous? for the time being I guess not
        auto context = reinterpret_cast<udp_send_context_t*>(req->data);
        if ( status < 0 )
        {
            std::printf("netUdpSocket:: Failed to send data to %s:%d: \"%s\"\n",
                context->ip,
                context->port,
                uv_strerror(status));
        }

        free(context->ip);
        free(req->data); // aka context
        free(req);
    }

    void netUdpSocket::send(const netPacket &packet)
    {
        uv_udp_send_t *req = (uv_udp_send_t *) malloc(sizeof(uv_udp_send_t));
        udp_send_context_t *context = (udp_send_context_t *) malloc(sizeof(udp_send_context_t));
        context->ip = (char *) malloc(packet.address.size() + 1);
        std::memcpy(context->ip, packet.address.c_str(), packet.address.size() + 1);

        context->port = packet.port;
        req->data = context;

        uv_buf_t *buf = (uv_buf_t *) malloc(sizeof(uv_buf_t));
        buf->base = (char *) malloc(packet.buffer_size);
        buf->len = packet.buffer_size;
        std::memcpy(buf->base, packet.buffer.data(), packet.buffer_size);

        sockaddr_in dest;
        uv_ip4_addr(packet.address.c_str(), packet.port, &dest);

        const auto result = uv_udp_send(req, &m_socket, buf, 1, reinterpret_cast<sockaddr*>(&dest), on_send_callback);
        if ( result != 0 )
        {
            std::printf("netUdpSocket:: Failed to send to %s:%d: \"%s\"\n",
                packet.address.c_str(), packet.port, uv_strerror(result));
        }
    }

    netUdpSocket::~netUdpSocket()
    {
        // free up the context
        free(m_socket.data);

        // now stop listening if the socket has been binded to an endpoint
        if ( m_port != -1 )
        {
            uv_udp_recv_stop(&m_socket);
        }

        // finally, close up the socket
        uv_close((uv_handle_t*)&m_socket, nullptr);
    }
}
