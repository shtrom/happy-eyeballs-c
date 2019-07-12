CFLAGS+=-Wall

TESTPORT=1025

TARGETS = \
	  connect_gai \
	  connect_rfc6555 \
	  connect_rfc6555_min \
	  # end
TEST_TARGETS = $(addprefix test-,$(TARGETS))

all: $(TARGETS)

connect_%: main.o %.o
	$(CC) $(CFLAGS) $^ -o $@

connect_rfc6555_min: rfc6555_lib.o

rfc6555_test: CFLAGS+=-g
rfc6555_test: rfc6555_lib.o

test%: CFLAGS+=-g
test: test-unit test-integration

test-unit: rfc6555_test
	./rfc6555_test

test-integration: $(TEST_TARGETS) test-down
test-%: % test-up
	./$(*) localhost $(TESTPORT)
test-up:
	-sudo iptables -I INPUT -p tcp --dport $(TESTPORT) -j ACCEPT
	-sudo ip6tables -I INPUT -p tcp --dport $(TESTPORT) -j DROP
	yes | nc -4klp $(TESTPORT) &
test-down:
	killall -9 nc  # XXX: dodgy
	-sudo ip6tables -D INPUT -p tcp --dport $(TESTPORT) -j DROP
	-sudo iptables -D INPUT -p tcp --dport $(TESTPORT) -j ACCEPT

clean:
	rm -f *.o
	rm -f rfc6555_test
	rm -f $(TARGETS)

.PHONY: all \
	test test-unit \
	test-integration test-up test-down \
	# $(TEST_TARGETS) \  # For some reason, those don't run if PHONY
	# end
