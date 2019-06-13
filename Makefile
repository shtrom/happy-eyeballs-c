TARGETS = \
	  connect_gai \
	  connect_rfc6555 \
	  # end

CFLAGS+=-Wall

all: $(TARGETS)

connect_%: main.o %.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o
	rm -f $(TARGETS)
