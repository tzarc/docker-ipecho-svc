IP Echo service
===

Run manually:
---

`docker run -it -e ECHO_UID=$(id -u) -e ECHO_GID=$(id -g) -e ECHO_PORTS="5000 6000" -p 5000:5000 -p 6000:6000 tzarc/ipecho-svc`

docker-compose:
---
```
version: '3'
services:
  ipecho-svc:
    image: tzarc/ipecho-svc:latest
    ports:
      - "5000:5000"
      - "6000:6000"
    environment:
      - "ECHO_UID=1000"
      - "ECHO_GID=1000"
      - "ECHO_PORTS=5000 6000"
```
