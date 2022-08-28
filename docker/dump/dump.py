from littlefs import LittleFS
import sys

if len(sys.argv) != 3:
    print("Two parameters needed: 'path to LittleFS binary' and 'path to output directory'")
    sys.exit()

inPath = sys.argv[1]
outDir = sys.argv[2]

fs = LittleFS(block_size=4096, block_count=368, mount=False)
with open(inPath, 'rb') as fh:
    fs.context.buffer = bytearray(fh.read())
fs.mount()
rootListing = fs.listdir('/')
for fileName in ["time.bin", "wifi_cred.dat", "timers.bin", "data.json"]:
    if fileName in rootListing:
        with fs.open('/' + fileName, 'rb') as fh:
            data = fh.read()
            with open(outDir + "/" + fileName, 'wb') as f:
                print(fileName + " stored")
                f.write(data)
