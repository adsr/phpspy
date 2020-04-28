FROM php:7.3-fpm-alpine
RUN apk add --update alpine-sdk python
RUN git clone --recursive https://github.com/adsr/phpspy.git
RUN cd phpspy && make
