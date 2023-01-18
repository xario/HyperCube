FROM alpine:3.16.2

RUN mkdir /src
RUN apk add --update git vim python3 py3-pip curl unzip bash libc6-compat libstdc++ gcompat screen
RUN pip3 install pyserial
RUN ln -s /usr/bin/python3 /usr/bin/python

RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh
COPY .arduino15 /root/.arduino15

RUN arduino-cli core update-index
RUN arduino-cli board listall
RUN arduino-cli core install esp32:esp32@1.0.5
RUN curl -sSL https://github.com/earlephilhower/esp-quick-toolchain/releases/download/3.0.4-gcc10.3/x86_64-linux-gnu.mklittlefs-943d2f7.210717.tar.gz | tar -xz -C /root/.arduino15/packages/esp32/tools/

RUN mkdir -p /root/Arduino/libraries
RUN git clone --depth 1 --branch v2.0.1 https://github.com/khoih-prog/AsyncHTTPSRequest_Generic /root/Arduino/libraries/AsyncHTTPSRequest_Generic
RUN git clone --depth 1 https://github.com/me-no-dev/AsyncTCP.git /root/Arduino/libraries/AsyncTCP-master
RUN git clone --depth 1 --branch v1.2.0 https://github.com/khoih-prog/AsyncTCP_SSL.git /root/Arduino/libraries/AsyncTCP_SSL
RUN git clone --depth 1 --branch 4.2.0 https://github.com/ThingPulse/esp8266-oled-ssd1306 /root/Arduino/libraries/esp8266-oled-ssd1306-master
RUN git clone --depth 1 --branch v1.1.2 https://github.com/khoih-prog/ESP_DoubleResetDetector /root/Arduino/libraries/ESP_DoubleResetDetector
RUN git clone --depth 1 --branch v1.12.2 https://github.com/khoih-prog/ESPAsync_WiFiManager /root/Arduino/libraries/ESPAsync_WiFiManager
RUN git clone --depth 1 https://github.com/me-no-dev/ESPAsyncWebServer.git /root/Arduino/libraries/ESPAsyncWebServer-master
RUN git clone --depth 1 --branch 0.1.3 https://github.com/xario/jsonlib /root/Arduino/libraries/jsonlib
RUN git clone --depth 1 --branch 1.0.6 https://github.com/lorol/LITTLEFS.git /root/Arduino/libraries/LittleFS_esp32
RUN git clone --depth 1 --branch 1.4.4 https://github.com/miguelbalboa/rfid.git /root/Arduino/libraries/MFRC522
RUN git clone --depth 1 --branch 1.2.0 https://github.com/jandrassy/StreamLib /root/Arduino/libraries/StreamLib
RUN git clone --depth 1 --branch 2.3.5 https://github.com/Links2004/arduinoWebSockets.git /root/Arduino/libraries/WebSockets
RUN git clone --depth 1 --branch v2.0.0 https://github.com/fbiego/ESP32Time.git /root/Arduino/libraries/ESP32Time-main
RUN git clone --depth 1 https://github.com/DIYDave/HTTP-DateTime.git /root/Arduino/libraries/HTTP-DateTime-master

RUN apk add --update nodejs-current yarn
#COPY React/*.json ./root/React/*.lock /root/React/
#ENV PATH /root/React/node_modules/.bin:$PATH
#WORKDIR /root/React/
#RUN yarn install --silent

COPY dump /root/dump
WORKDIR /root/dump
RUN pip install --no-cache-dir -r requirements.txt

#COPY React/public /root/React/public
#COPY React/src /root/React/src
#WORKDIR /root/React/
#RUN yarn run build

COPY Arduino/hypercube /root/Arduino/hypercube

COPY build-and-upload.sh /root/build-and-upload.sh
RUN chmod +x /root/build-and-upload.sh

COPY monitor.sh /root/monitor.sh
RUN chmod +x /root/monitor.sh

COPY backup.sh /root/backup.sh
RUN chmod +x /root/backup.sh

COPY restore.sh /root/restore.sh
RUN chmod +x /root/restore.sh

WORKDIR /root

CMD ["bash", "/root/build-and-upload.sh"]
