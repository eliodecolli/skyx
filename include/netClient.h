/*
 * File Name: netClient.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Backbone client communication.
 */

#include <uv.h>
#include <string>
#include <netPacket.h>
#include <future>
#include <common.h>


namespace skyx
{
    /*
     * netClient - Represents the backbone communication component between skyX peers.
     * NOTE:    Unlike netServer which owns its own background thread,
     *          and joins it to block the main process thread,
     *          this one is supposed to be host under a main thread.
     *
     *          Therefore it's the caller's job to initialize the underlying uv_loop, and own it.
     */
    class netClient
    {
        using callback_fn = cb<netClient, netPacket>;
        using connect_callback_fn = std::function<void(netClient*, bool)>;

        struct client_context_t {
            netClient   *owner;
        };

        struct cleanup_context_t {
            client_context_t *context;
            std::promise<bool> prom;
        };

        private:
            uv_loop_t                           *m_loop;
            uv_tcp_t                            m_client_handle;

            bool                                m_connected;
            endpoint_t                          m_server_endpoint;

        private:
            callback_fn                     m_rcv;
            connect_callback_fn             m_connect_cb;

        private:
            static void connect_callback(uv_connect_t *req, int status);
            static void on_write_complete(uv_write_t *req, int status);
            static void on_client_receive_internal(uv_stream_t *client, ssize_t nread, const uv_buf_t *buff);
            static void on_buffer_allocation(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

        private:
            void assert_connected();
            void cleanup_client();

        public:
            bool connect(const std::string &address_ip, int address_port, const connect_callback_fn &connect_cb);
            void send(const netPacketBuffer &buf);
            void on_receive(callback_fn func);

        public:
            netClient(uv_loop_t *event_loop);
            ~netClient();
    };
}
