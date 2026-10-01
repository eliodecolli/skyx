LIBUV = ../build/ext/libuv/libuv/libuv.a

compile-server:
	g++ test_server.cpp netServer.cpp $(LIBUV) -o test_server -std=c++26 -I../include/ -I../ext/libuv/libuv/include/ -fsanitize=address -g -fno-omit-frame-pointer

compile-client:
	g++ test_client.cpp netClient.cpp $(LIBUV) -o test_client -std=c++26 -I../include/ -I../ext/libuv/libuv/include/ -fsanitize=address -g -fno-omit-frame-pointer

compile-udp-server:
	g++ test_udp_server.cpp netUdpSocket.cpp $(LIBUV) -o test_udp_server -std=c++26 -I../include/ -I../ext/libuv/libuv/include/ -fsanitize=address -g -fno-omit-frame-pointer

compile-udp-client:
	g++ test_udp_client.cpp netUdpSocket.cpp $(LIBUV) -o test_udp_client -std=c++26 -I../include/ -I../ext/libuv/libuv/include/ -fsanitize=address -g -fno-omit-frame-pointer

compile: compile-server compile-client compile-udp-server compile-udp-client

cleanup-tcp:
	rm test_client
	rm test_server
	rm -r test_server.dSYM
	rm -r test_client.dSYM

cleanup-udp:
	rm test_udp_client
	rm test_udp_server
	rm -r test_udp_server.dSYM
	rm -r test_udp_client.dSYM

cleanup: cleanup-tcp cleanup-udp
