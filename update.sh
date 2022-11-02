#!/bin/sh
HAD_LOGGING=false
if docker ps | grep hyperlog -q; then
  HAD_LOGGING=true
  docker rm -f hyperlog &>/dev/null
  sleep 3
fi

docker build ./docker -t hyper:latest
docker run -it --rm --privileged hyper:latest

if $HAD_LOGGING; then
  bash start-log.sh &>/dev/null &
fi