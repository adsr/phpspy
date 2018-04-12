all: phpspy

phpspy: phpspy.c
	$(CC) -Wall -Wextra -ldw -g -I. phpspy.c -o phpspy

phpspy_zend: phpspy.c
	$(CC) -Wall -Wextra -ldw -g $$(php-config --includes) -DUSE_ZEND=1 -I. phpspy.c -o phpspy

clean:
	rm -f phpspy

.PHONY: phpspy_zend clean
