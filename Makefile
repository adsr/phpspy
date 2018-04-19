all: phpspy

phpspy: phpspy.c
	$(CC) -Wall -Wextra -g -I. phpspy.c -o phpspy -ldw

phpspy_zend: phpspy.c
	$(CC) -Wall -Wextra -g $$(php-config --includes) -DUSE_ZEND=1 -I. phpspy.c -o phpspy -ldw

clean:
	rm -f phpspy

.PHONY: phpspy_zend clean
