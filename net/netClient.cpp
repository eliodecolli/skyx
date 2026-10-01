/*
 * File Name: netClient.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 23/09/2026
 * Purpose: Implements asynchronous backbone client communication.
 */

#include "netPacket.h"
#include "uv.h"
#include "uv/unix.h"
#include <netClient.h>
#include <netinet/in.h>
#include <stdexcept>

namespace skyx
{
    void netClient::connect_callback(uv_connect_t* req, int status)
    {
        auto context = ((client_context_t*)req->data);
        auto handle = [context] (bool value)
            {
                context
                    ->owner
                        ->m_connected = value;
            };

        if ( status < 0 )
        {
            std::printf(
                "netClient:: Error connecting to server: %s\n",
                uv_strerror(status)
            );
            handle(false);
        }

        uv_read_start(
            (uv_stream_t*)&context->owner->m_client_handle,
            on_buffer_allocation,
            on_client_receive_internal
        );
        handle(true);
        free(req);
    }

    bool netClient::connect(const std::string &ip, int port)
    {
        uv_tcp_init(m_loop, &m_client_handle);

        auto *client_context = new client_context_t { this };
        m_client_handle.data = client_context;

        sockaddr_in addr;
        uv_ip4_addr(ip.c_str(), port, &addr);

        uv_connect_t *m_connect_handle =
            (uv_connect_t *) malloc(sizeof(uv_connect_t));
        m_connect_handle->data = client_context;

        uv_tcp_connect(
            m_connect_handle,
            &m_client_handle,
            reinterpret_cast<const struct sockaddr*>(&addr),
            connect_callback
        );

        m_server_endpoint.t_ip = ip;
        m_server_endpoint.t_port = port;

        return true;
    }

    void netClient::on_write_complete(uv_write_t* req, int status)
    {
        if ( status < 0 )
        {
            std::printf(
                "netClient:: Error sending data to server: %s",
                uv_strerror(status)
            );
        }

        //free(req->bufs->base);
        free(req->bufs);
        free(req);
    }

    void netClient::assert_connected()
    {
        if ( !m_connected )
        {
            std::printf("netClient:: Cannot send data while client connection is not established\n");
            throw std::runtime_error("netClient: Connection hasn't been established yet.");
        }
    }

    void netClient::send(const netPacketBuffer &n_buf)
    {
        assert_connected();

        uv_write_t *write_handle =
            (uv_write_t *) malloc(sizeof(uv_write_t));

        auto len = n_buf.size();
        uv_buf_t *buf = (uv_buf_t *) malloc(sizeof(uv_buf_t));
        buf->base = (char *) malloc(len);
        buf->len = len;
        memcpy(buf->base, n_buf.data(), len);

        uv_write(
            write_handle,
            (uv_stream_t *)&m_client_handle,
            buf,
            1,
            on_write_complete
        );
    }

    void netClient::on_buffer_allocation(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
    {
        buf->base = (char *) malloc(suggested_size);
        buf->len = suggested_size;
    }

    void netClient::cleanup_client()
    {
        uv_close((uv_handle_t*)&m_client_handle, [] (uv_handle_t *handle) {
            // clean up pointer to self
            auto prom = static_cast<std::promise<bool>*>(handle->data);
            prom->set_value(true);
        });
    }

    void netClient::on_client_receive_internal(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
    {
        auto context = (client_context_t *) client->data;
        auto m_rcv = context->owner->m_rcv;

        if (nread < 0 || nread == UV_EOF)
        {
            std::printf(
                "netClient:: Connection to server has been lost\n"
            );

            context->owner->cleanup_client();
            return;
        }

        std::printf(
            "netClient:: Received %lu bytes from server\n",
            nread
        );

        netPacketBuffer temp_buf;
        for (ssize_t i = 0; i < nread; i++)
        {
            uint8_t cur = static_cast<uint8_t>(*(buf->base + i));
            temp_buf.emplace_back(cur);
        }

        if ( m_rcv == nullptr )
        {
            std::printf("netClient:: Receive callback not defined. Ignoring message.\n");
            return;
        }

        netPacket packet;
        packet.buffer = std::move(temp_buf);
        packet.buffer_size = temp_buf.size();
        packet.address = context->owner->m_server_endpoint.t_ip;
        packet.port = context->owner->m_server_endpoint.t_port;

        m_rcv(context->owner, std::move(packet));
    }

    void netClient::on_receive(callback_fn callback)
    {
        m_rcv = std::move(callback);
    }

    netClient::~netClient()
    {
        std::printf(
            "netClient:: Instance %p closing\n",
            this
        );
        uv_read_stop((uv_stream_t*)&m_client_handle);
        std::promise<bool> prom;

        // the old switcheroo
        client_context_t *ctx = static_cast<client_context_t*>(m_client_handle.data);
        m_client_handle.data = &prom;
        auto future = prom.get_future();

        cleanup_client();

        auto result = future.get();
        if ( !result )
        {
            // wat
            std::printf("netClient:: What the fuck?\n");
        }

        delete ctx;
    }

    netClient::netClient(uv_loop_t *event_loop)
    {
        m_loop = event_loop;
    }
}
