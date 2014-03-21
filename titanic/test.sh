#!/bin/bash

gcc -O2 -lzmq -lczmq mdbroker.c -o mdbroker && \
gcc -O2 -lzmq -lczmq ticlient.c -o ticlient && \
gcc -O2 -lzmq -lczmq mdworker.c -o mdworker && \
gcc -O2 -lzmq -lczmq -luuid titanic.c -o titanic
