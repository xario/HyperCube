#!/bin/bash
trap 'echo Bye!; exit' INT
PORT=$(arduino-cli board list | tail -n2 | grep tty | cut -d" " -f1)
if ! [[ "$PORT" =~ /dev/* ]]; then
  echo "No HyperCube found. Make sure to plug in the Cube before using the tool"
  exit 1
fi

echo "Staring monitoring of $PORT..."
touch /tmp/out.log
screen -dm bash -c "exec bash -c \"arduino-cli monitor --port $PORT -c baudrate=115200\" >> /tmp/out.log"
tail -f /tmp/out.log -n 10000 &
until pgrep arduino-cli > /dev/null; do
  sleep 1
done
while pgrep arduino-cli > /dev/null; do
  sleep 1
done