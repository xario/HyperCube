#!/bin/sh
HAD_LOGGING=false
if docker ps | grep hyperlog -q; then
  HAD_LOGGING=true
  docker rm -f hyperlog &>/dev/null
fi

docker build ./docker -t hyper:latest
mkdir ~/hypercube-backup
docker run -it -v ~/hypercube-backup:/tmp/backup --rm --privileged hyper:latest bash /root/backup.sh

