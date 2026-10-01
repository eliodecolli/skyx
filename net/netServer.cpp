/*
 * File Name: netServer.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Implements the libuv-backed TCP server.
 */

#include <netServer.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <printf.h>
#include <string>
#include <format>
#include <unistd.h>
#include <cstring>

namespace skyx {

    void netServer::on_recieve(callback_fn callback) {
        m_callback = std::move(callback);
    }

    netClientName netServer::get_client_name(std::string ip, int port) {
        return std::format("{}:{}", ip, port);
    }

    void cleanup(uv_handle_t *handle) {
        if ( uv_is_closing(handle) ) return;
        uv_close(handle, [] (uv_handle_t *p_handle) {
            free(p_handle);
        });
    }

    uv_loop_t *netServer::get_loop()
    {
        return m_loop;
    }

    void netServer::quit_server() {
        if ( m_shutdown ) return;

        uv_close((uv_handle_t*)&m_server_handle, nullptr);
        uv_close((uv_handle_t*)&m_sigint, nullptr);

        for (auto &sp : m_clients) {
            auto &c = sp.second;
            if (!uv_is_closing((uv_handle_t*)&c->m_client))
                uv_close((uv_handle_t*)&c->m_client, nullptr);
        }
        m_clients.clear();

        uv_walk(m_loop, [](uv_handle_t* h, void*) {
            cleanup(h);
        }, nullptr);

        uv_loop_close(m_loop);
        free(m_loop);
        m_shutdown = true;
    }

    netServer::netServer() {
        m_shutdown = false;

        m_loop = (uv_loop_t*) malloc(sizeof(uv_loop_t));
        uv_loop_init(m_loop);

        uv_signal_init(m_loop, &m_sigint);
        m_sigint.data = this;
        uv_signal_start(&m_sigint, on_signal, SIGINT);

        printf("netServer:: uv initialized\n");
    }

    netServer::~netServer() {
        quit_server();
    }

    void netServer::on_signal(uv_signal_t *handle, int signum) {
        std::printf("netServer:: Caught signal %d, shutting down...\n", signum);
        auto net_server = static_cast<netServer*>(handle->data);
        net_server->quit_server();
    }

    void netServer::listen(int port) {
        m_port = port;
        m_core_thread = std::jthread([this] () {
            uv_tcp_init(m_loop, &m_server_handle);
            sockaddr_in addr;
            uv_ip4_addr("0.0.0.0", m_port, &addr);

            uv_tcp_bind(&m_server_handle, reinterpret_cast<const sockaddr*>(&addr), 0);

            m_server_handle.data = this;
            const int listen_result = uv_listen((uv_stream_t*)&m_server_handle, 100, on_new_connection);

            if (listen_result != 0) {
                printf("netServer:: Listening error: %s\n", uv_strerror(listen_result));
                return;
            }

            printf("netServer:: Started listening\n");
            uv_run(m_loop, UV_RUN_DEFAULT);
        });

        m_core_thread.join();
    }

    void netServer::on_new_connection(uv_stream_t *server, int status) {
        auto net_server = static_cast<netServer*>(server->data);
        if (status < 0) {
            printf("netServer:: Error while accepting new connection: %s\n", uv_strerror(status));
            return;
        }

        auto net_client = std::make_unique<uv_netClient_t>();
        uv_tcp_init(net_server->m_loop, &net_client->m_client);
        const int accept_result = uv_accept(server, (uv_stream_t*)&net_client->m_client);

        if ( accept_result != 0 ) {
            printf("netServer:: Error while accepting TCP connection: %s\n", uv_strerror(accept_result));
            return;
        }

        sockaddr_storage addr_storage;
        int len_addr_storage = sizeof(addr_storage);
        const int get_name_result = uv_tcp_getpeername(&net_client->m_client, (sockaddr*)&addr_storage, &len_addr_storage);

        if ( get_name_result != 0 ) {
            printf("netServer:: Error while trying to query client name: %s\n", uv_strerror(get_name_result));
            uv_close((uv_handle_t*)&net_client->m_client, NULL);
            return;
        }

        const sockaddr_in *client_addr = reinterpret_cast<const sockaddr_in*>(&addr_storage);
        char client_ip[INET6_ADDRSTRLEN];
        const int client_port = ntohs(client_addr->sin_port);
        uv_ip4_name(client_addr, client_ip, sizeof(client_ip));

        const netClientName name = net_server->get_client_name(client_ip, client_port);

        net_client->m_owner = net_server;
        net_client->m_ip = client_ip;
        net_client->m_port = client_port;

        // some recursive black fuckery
        net_client->m_client.data = net_client.get();

        auto client = &net_client.get()->m_client;
        net_server->m_clients.emplace(name, std::move(net_client));

        uv_read_start((uv_stream_t*)client, on_buffer_allocation, on_client_receive);

        printf("netServer:: Accepted connection from %s\n", name.c_str());
    }

    void netServer::on_client_receive(uv_stream_t *client, ssize_t nread, const uv_buf_t *buff)
    {
        auto net_client = static_cast<uv_netClient_t*>(client->data);
        if ( nread > 0 ) {
            netPacket packet;
            packet.address = net_client->m_ip;
            packet.port = net_client->m_port;

            for (int i = 0; i < nread; i++) {
                auto cur = *((uint8_t*)(buff->base + i));
                packet.buffer.emplace_back(cur);
            }
            packet.buffer_size = nread;

            netServer *server = net_client->m_owner;
            if ( server->m_callback == nullptr )
            {
                std::printf("netServer:: Receive callback is not defined. Ignoring packet.\n");
                return;
            }
            server->m_callback(server, std::move(packet));
        }

        if ( nread < 0 || nread == UV_EOF ) {
            uv_close((uv_handle_t*)client, [] (uv_handle_t* handle) {
                auto net_client = static_cast<uv_netClient_t*>(handle->data);
                netClientName name = net_client->m_owner->get_client_name(net_client->m_ip, net_client->m_port);
                std::printf("Client %s disconnected\n", name.c_str());

                net_client->m_owner->m_clients.erase(name);  // this will free the memory too
            });
        }

        free(buff->base);
    }

    void netServer::on_buffer_allocation(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
    {
        buf->base = (char*) malloc(suggested_size);
        buf->len = suggested_size;
    }

    void netServer::on_write_complete(uv_write_t* req, int status) {
        if ( status != 0 ) {
            printf("Error while trying to send message to client: %s\n", uv_strerror(status));
        }

        auto buffer = static_cast<uv_buf_t*>(req->data);
        std::printf("Sent %zu bytes\n", buffer->len);

        free(buffer->base);
        free(buffer);
        free(req);
    }

    void netServer::send_data(const netPacket &data)
    {
        const netClientName name = get_client_name(data.address, data.port);
        auto it = m_clients.find(name);
        if (it == m_clients.end()) {
            printf("netServer:: Error client %s not found\n", name.c_str());
            return;
        }
        auto &client = it->second;

        uv_buf_t *buffer = (uv_buf_t*) malloc(sizeof(uv_buf_t));
        buffer->base = (char*) malloc(data.buffer_size);

        memcpy(buffer->base, data.buffer.data(), data.buffer_size);
        buffer->len = data.buffer_size;

        uv_write_t *req = (uv_write_t*) malloc(sizeof(uv_write_t));
        req->data = buffer;

        uv_write(req, (uv_stream_t *)&client->m_client, buffer, 1, on_write_complete);
    }
}
