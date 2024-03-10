include ../config.mk

all: build run

run: build
	@ find . -maxdepth 1 -type f -executable | while read test_file; do \
			echo -e "===\n=== starting $$test_file\n==="; \
			./$$test_file || :; \
		done;

build: test_queue test_runnsctl

test_%: ../%.c %.c
	$(CC) -DTAU_TEST -I.. -I../tau/ -o test_$@ $^

.PHONY: clean
clean:
	find . -maxdepth 1 -executable -type f -delete
