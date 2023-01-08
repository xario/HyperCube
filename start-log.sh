#!/bin/bash
trap 'docker rm -f hyperlog; echo Bye!; exit' INT
until systemctl status docker | grep running -q; do
  sleep 1
done

docker rm -f hyperlog &>/dev/null
docker build ./docker -t hyper:latest
bash -c "docker run --rm --name hyperlog -i --privileged hyper:latest bash /root/monitor.sh" &
until docker ps | grep hyperlog -q; do
  sleep 1
done
if [ -f /home/$USER/cube.log ]; then
  chown $USER:$USER /home/$USER/cube.log
fi
docker logs --timestamps --follow hyperlog >> /home/$USER/cube.log