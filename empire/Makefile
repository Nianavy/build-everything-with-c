TARGET = bin/db
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

run: clean default
		./$(TARGET) -f test.db -n
		./$(TARGET) -f test.db -a "Freddy Krueger.-1428 Elm Street.-10000"
		./$(TARGET) -f test.db -l

default: $(TARGET)

clean:
		rm -f obj/*.o
		rm -f bin/*
		rm -f *.db 

$(TARGET): $(OBJ)
		gcc -Wall -o $@ $?

obj/%.o : src/%.c
		gcc -c $< -o $@ -Iinclude
