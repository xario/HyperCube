#!/bin/bash
trap 'echo Bye!; exit' INT
set -e
PORT=$(arduino-cli board list | tail -n2 | grep tty | cut -d" " -f1)
if ! [[ "$PORT" =~ /dev/* ]]; then
  echo "No HyperCube found. Make sure to plug in the Cube before using the tool"
  exit 1
fi

SPI_PAGE=$(echo $((16#100)))
SPI_BLOCK=$(echo $((16#1000)))
SPI_START=$(echo $((16#290000)))
SPI_END=$(echo $((16#400000)))
SPI_SIZE=$(expr $SPI_END - $SPI_START)
DATA_DIR="/root/Arduino/hypercube/data"
REACT_BUILD_DIR="/root/React/build"
DUMP_BIN="/tmp/dump.bin"
DATA_BIN="/tmp/data.bin"

echo "####### Backing up current configuration... #######"
python /root/.arduino15/packages/esp32/tools/esptool_py/3.0.0/esptool.py -b 921600 read_flash $SPI_START $SPI_SIZE $DUMP_BIN
echo "####### Exporting configuration from backup #######"
python /root/dump/dump.py $DUMP_BIN $DATA_DIR
NAME_PATH="$DATA_DIR/name"
if [ -f "$NAME_PATH" ]; then
  NAME=`cat $NAME_PATH`
else
  echo "Enter the number of the HyperCube to be updated:"
  read CUBE_NUMBER
  until [[ $CUBE_NUMBER =~ ^[1-9]$ ]] ; do
    echo "Unable to parse the number, please try again. Enter a number from 1 to 9"
    read CUBE_NUMBER
  done
  echo "Number registered. HyperCube${CUBE_NUMBER} is now being updated..."
  NAME="hypercube${CUBE_NUMBER}"
  echo "$NAME" > $NAME_PATH
fi

BACKUP_DIR="/tmp/backup/${NAME}/"
mkdir -p $BACKUP_DIR
cp -r $DATA_DIR $BACKUP_DIR
