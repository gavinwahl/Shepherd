shepherd: shepherd.c
	$(CC) $(CFLAGS) -o shepherd shepherd.c

clean:
	-rm shepherd

install: shepherd
	cp shepherd /usr/bin/shepherd

.PHONY: clean
