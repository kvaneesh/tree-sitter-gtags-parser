TARGET = tree-sitter-query.so
LDFLAGS = -shared -fPIC -DPIC -ltree-sitter -ldl
CFLAGS =  -g -I$(GTAGS_DIR)/libparser/   -fPIC -DPIC -Wall

all: $(TARGET)
clean:
	rm $(TARGET)

%.so: %.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
