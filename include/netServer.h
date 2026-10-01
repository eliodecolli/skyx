/*
 * File Name: netServer.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Declares the libuv-backed network server.
 */

#pragma once

#include <netPacket.h>
#include <functional>
#include <unordered_map>
#include <thread>
#include <uv.h>

namespace skyx
{
    class netServer {
        using callback_fn = std::function<void(netServer*, netPacket)>;

    private:
        struct uv_netClient_t {
            netServer *m_owner;
            uv_tcp_t m_client;
            std::string m_ip;
            int m_port;
        };

        uv_loop_t       *m_loop;
        uv_tcp_t        m_server_handle;
        uv_signal_t     m_sigint;

        int             m_port;

        bool            m_shutdown;

        std::jthread    m_core_thread;

        std::unordered_map<netClientName, std::unique_ptr<uv_netClient_t>> m_clients;
    private:
        static void on_new_connection(uv_stream_t *server, int status);
        static void on_client_receive(uv_stream_t *client, ssize_t nread, const uv_buf_t *buff);
        static void on_buffer_allocation(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
        static void on_write_complete(uv_write_t* req, int status);
        static void on_signal(uv_signal_t *handle, int signum);

        void quit_server();

    private:
        callback_fn m_callback;

    private:
        netClientName get_client_name(std::string ip, int port);

    public:
        void listen(int port);
        void on_recieve(callback_fn callback);
        void send_data(const netPacket &data);

        netServer();
        virtual ~netServer();
    };

}
