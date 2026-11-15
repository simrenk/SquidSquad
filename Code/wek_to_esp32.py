import serial
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import BlockingOSCUDPServer

esp = serial.Serial()
esp.port = "/dev/cu.usbserial-0001"
esp.baudrate = 115200
esp.timeout = 0
esp.dtr = False   # don't touch the ESP32's reset line
esp.rts = False   # don't touch the ESP32's boot line
esp.open()

last_gesture = None

def on_output(address, *args):
    global last_gesture
    gesture = int(round(args[0]))

    if gesture != last_gesture:
        print("Received:", gesture)
        last_gesture = gesture

    esp.write(str(gesture).encode())

    while esp.in_waiting:
        line = esp.readline().decode(errors="ignore").strip()
        if line:
            print("ESP32:", line)

dispatcher = Dispatcher()
dispatcher.map("/wek/outputs", on_output)

print("Bridge v4 running")
BlockingOSCUDPServer(("127.0.0.1", 12000), dispatcher).serve_forever()