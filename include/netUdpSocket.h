#include <netPacket.h>
#include <common.h>
#include <uv.h>


namespace skyx
{
    class netUdpSocket
    {
        using callback_fn = cb<netUdpSocket, netPacket>;

        struct udp_send_context_t {
            char    *ip;
            int     port;
        };

        struct udp_socket_context_t {
            netUdpSocket    *owner;
        };

      private:
        uv_loop_t       *m_loop;
        uv_udp_t        m_socket;
        callback_fn     m_callback;
        int             m_port = -1;

      private:
          static void on_send_callback(uv_udp_send_t* req, int status);
          static void on_buffer_allocation(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
          static void on_udp_receive(uv_udp_t* handle,
                                    ssize_t nread,
                                    const uv_buf_t* buf,
                                    const struct sockaddr* addr,
                                    unsigned flags);

      public:
          void on_receive(callback_fn callback);
          void send(const netPacket &packet);
          void bind(const int port);
          void start();

      public:
          netUdpSocket(uv_loop_t *loop);
          ~netUdpSocket();
    };
}
