compile:
	g++ -std=c++26 test_serializer.cpp skySerialize.cpp -o test_serializer -fsanitize=address -g -fno-omit-frame-pointer -I/usr/local/opt/boost/include -I../../ext/libuv/libuv/include -I../../include

run-test-job: compile
	./test_serializer

clean:
	rm ./test_serializer
	rm -r test_serializer.dSYM

run-tests: run-test-job clean
