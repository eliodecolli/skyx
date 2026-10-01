#include <netPacket.h>
#include <netUdpSocket.h>
#include <uv.h>

#include <cstdio>
#include <string>
#include <utility>

void on_receive(skyx::netUdpSocket *server, const skyx::netPacket &packet)
{
    std::string message;
    for( auto &c : packet.buffer ) {
        message += static_cast<char>(c);
    }

    printf("Client %s:%d: %s\n", packet.address.c_str(), packet.port, message.c_str());

    skyx::netPacketBuffer buf;
    buf.insert(buf.begin(), packet.buffer.begin(), packet.buffer.end());

    auto response = packet.make_response(std::move(buf), packet.buffer_size);
    server->send(response);
    printf("Sent to client %s:%d\n", packet.address.c_str(), packet.port);
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    uv_loop_t loop;
    if (uv_loop_init(&loop) < 0)
    {
        return 1;
    }

    skyx::netUdpSocket server { &loop };
    server.on_receive(on_receive);
    server.bind(8080);
    server.start();
    uv_run(&loop, UV_RUN_DEFAULT);
    return 0;
}
