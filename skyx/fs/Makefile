compile-test-job:
	g++-16 test_job.cpp job.cpp -o test_job -fsanitize=address -g -fno-omit-frame-pointer -DSKYX__TEST

run-test-job: compile-test-job
	./test_job

clean:
	rm ./test_job
	rm -r test_job.dSYM

run-tests: run-test-job clean
