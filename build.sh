#!/usr/bin/env bash

rm -rf ./scan-build*

docker build --target builder --tag tzarc/ipecho-svc:builder .
docker build --target tester --tag tzarc/ipecho-svc:tester .
docker build --tag tzarc/ipecho-svc:latest .

CONTAINER_ID=$(docker create tzarc/ipecho-svc:builder)
docker cp $CONTAINER_ID:/src/scan-build.tar.gz . || true
docker rm $CONTAINER_ID

tar -zxf scan-build.tar.gz --strip-components=1 || true