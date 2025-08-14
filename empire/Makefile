TARGET = bin/db
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))
CFLAGS = -Iinclude -Wall -g
LDFLAGS =

.PHONY: all run default clean

all: default

run: default
	@echo "--- Running dbview operations ---"
	./$(TARGET) -f test.db -n
	./$(TARGET) -f test.db -a "Freddy Krueger.,-1428 Elm Street.,10000"
	./$(TARGET) -f test.db -l
	@echo "--- Operations complete ---"
	@echo "Checking database file content (using hexdump for binary check):"
	hexdump -C test.db


default: $(TARGET)

clean:
	@echo "--- Cleaning up ---"
	rm -f $(OBJ)
	rm -f $(TARGET)
	rm -f *.db
	@echo "--- Cleanup complete ---"


$(TARGET): $(OBJ)
	@echo "--- Linking $@ ---"
	gcc $(LDFLAGS) -o $@ $^

obj/%.o: src/%.c
	@echo "--- Compiling $< ---"
	gcc $(CFLAGS) -c $< -o $@