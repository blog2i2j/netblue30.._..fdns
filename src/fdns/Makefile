all: fdns

include ../common.mk

%.o : %.c $(H_FILE_LIST)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDE) -c $< -o $@

fdns: $(OBJS)
	$(CC)  $(LDFLAGS) -o $@ $(OBJS)  -lssl -lcrypto -lrt -lseccomp -lm $(LIBS) $(EXTRA_LDFLAGS)

clean:; rm -f *.o fdns *.gcov *.gcda *.gcno

distclean: clean
