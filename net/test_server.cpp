#include <netPacket.h>
#include <netServer.h>

void on_receive(skyx::netServer *server, const skyx::netPacket &packet)
{
    std::string message;
    for( auto &c : packet.buffer ) {
        message += static_cast<char>(c);
    }

    printf("Client %s:%d: %s\n", packet.address.c_str(), packet.port, message.c_str());

    skyx::netPacketBuffer buf;
    buf.insert(buf.begin(), packet.buffer.begin(), packet.buffer.end());

    auto response = packet.make_response(std::move(buf), packet.buffer_size);
    server->send_data(response);
    printf("Sent to client %s:%d\n", packet.address.c_str(), packet.port);
}

int main()
{
    // Keep diagnostics visible when a debugger captures stdout or the server crashes.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    skyx::netServer server;
    server.on_recieve(on_receive);
    server.listen(8080);
    return 0;
}
