FROM php:8.1-fpm-alpine

RUN apk add --update alpine-sdk \
    && git clone --recursive https://github.com/adsr/phpspy.git \
    && cd phpspy \
    && make
