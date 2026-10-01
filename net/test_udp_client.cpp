#include <netPacket.h>
#include <netUdpSocket.h>
#include <uv.h>

#include <cstdio>
#include <string>
#include <string_view>

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    uv_loop_t loop;
    if (uv_loop_init(&loop) < 0)
    {
        return 1;
    }

    const std::string_view messages[] = {"Hi there", "Hello World"};
    std::size_t message_index = 0;

    auto get_packet = [] (std::string_view s) {
        skyx::netPacket packet;
        packet.address = "127.0.0.1";
        packet.port = 8080;
        for (auto &c : s)
        {
            packet.buffer.emplace_back(c);
        }
        packet.buffer_size = packet.buffer.size();
        return packet;
    };

    skyx::netUdpSocket client { &loop };
    client.on_receive([&] (skyx::netUdpSocket *sender,
                           const skyx::netPacket &packet) {
        auto &buf = packet.buffer;
        if (buf.empty() || message_index >= 2)
        {
            return;
        }

        const std::string response(buf.begin(), buf.end());
        std::printf("Server: %s\n", response.c_str());
        if (response != messages[message_index])
        {
            std::fprintf(stderr, "Unexpected echo: expected \"%.*s\"\n",
                         static_cast<int>(messages[message_index].size()),
                         messages[message_index].data());
            return;
        }

        ++message_index;
        if (message_index < 2)
        {
            sender->send(get_packet(messages[message_index]));
            std::printf("Sent data to server again\n");
        }
        else
        {
            std::printf("Both UDP echoes verified\n");
        }
    });

    client.bind(0);
    client.start();
    client.send(get_packet(messages[message_index]));
    std::printf("Sent data to server\n");
    uv_run(&loop, UV_RUN_DEFAULT);
    return 0;
}
