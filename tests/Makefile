include ../config.mk

all: test_queue test_runns

run: run_queue run_runns

test_queue: test_queue.c ../queue.c
	$(CC) -DTAU_TEST -I.. -I../tau/ -o $@ $^
run_queue:
	./test_queue || :

test_runns: test_$(CLIENT).c ../$(CLIENT).c
	$(CC) -DTAU_TEST -I.. -I../tau -o $@ $^
run_runns:
	./test_runns || :

.PHONY: clean
clean:
	find . -maxdepth 1 -executable -type f -delete
