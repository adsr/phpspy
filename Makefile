all: phpspy

phpspy: phpspy.c
	$(CC) -Wall -Wextra -g $$(php-config --includes) phpspy.c -o phpspy

clean:
	rm -f phpspy

.PHONY: clean
