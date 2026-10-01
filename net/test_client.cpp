#include <netPacket.h>
#include <uv.h>
#include <netClient.h>

#include <cstdio>
#include <future>
#include <string>
#include <string_view>
#include <thread>

int main()
{
    uv_loop_t loop;
    if (uv_loop_init(&loop) < 0)
    {
        return 1;
    }

    uv_idle_t idle;
    uv_async_t shutdown;
    std::jthread t;

    std::promise<void> received;
    auto finished = received.get_future();

    const std::string_view messages[] = {"Hi there", "Hello World"};
    std::size_t message_index = 0;

    auto get_buf = [] (std::string_view s) {
        skyx::netPacketBuffer buf;
        for (auto &c : s)
        {
            buf.emplace_back(c);
        }
        return buf;
    };

    {
        skyx::netClient client { &loop };

        client.on_receive([&] (skyx::netClient *sender,
                               const skyx::netPacket packet) {
            auto &buf = packet.buffer;
            if (buf.empty() || message_index >= 2)
            {
                return;
            }

            // This smoke test assumes one complete small echo per callback.
            const std::string response(buf.begin(), buf.end());
            std::printf("Server: %s\n", response.c_str());

            ++message_index;
            if (message_index < 2)
            {
                sender->send(get_buf(messages[message_index]));
                std::printf("Sent data to server again\n");
            }
            else
            {
                received.set_value();
            }
        });

        client.connect("127.0.0.1", 8080);

        // Process the connection callback before sending or adding keepalives.
        uv_run(&loop, UV_RUN_ONCE);

        uv_idle_init(&loop, &idle);
        uv_idle_start(&idle, [] (uv_idle_t*) {});

        uv_async_init(&loop, &shutdown, [] (uv_async_t *handle) {
            auto *idle = static_cast<uv_idle_t*>(handle->data);
            uv_idle_stop(idle);
            uv_close(reinterpret_cast<uv_handle_t*>(idle), nullptr);
            uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
        });
        shutdown.data = &idle;

        client.send(get_buf(messages[message_index]));
        std::printf("Sent data to server\n");

        t = std::jthread([&loop] () {
            uv_run(&loop, UV_RUN_DEFAULT);
        });

        finished.get();

        // The current client destructor waits for the loop's close callback.
    }

    uv_async_send(&shutdown);
    t.join();

    int status = uv_loop_close(&loop);
    if (status < 0)
    {
        std::fprintf(stderr, "uv_loop_close: %s\n", uv_strerror(status));
        return 1;
    }

    return 0;
}
