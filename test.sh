#!/usr/bin/env bash

./build.sh
docker run --rm -it -e ECHO_UID=$(id -u) -e ECHO_GID=$(id -g) -e ECHO_PORTS="45000" -p 45000:45000 tzarc/ipecho-svc:tester valgrind /ipecho-svc.dbg