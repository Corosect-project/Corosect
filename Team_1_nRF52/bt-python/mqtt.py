#!.venv/bin/python3
import paho.mqtt.client as mqtt

from logger import Logger

log = Logger(__name__)


class MqttHandler:
    exit: bool = False
    client = mqtt.Client("Linux BLE handler")

    def __init__(self) -> None:
        self.client.on_connect = self.onConnect
        self.client.on_message = self.onMessage

    def __enter__(self):
        self.client.connect("127.0.0.1")
        self.client.loop_start()
        return self

    def __exit__(self, type, value, traceback):
        log.debug("Stopping mqtt")
        self.client.loop_stop()
        self.client.disconnect()

    def onConnect(self, client: mqtt.Client, *args):
        client.subscribe("CONTROL")

    def onMessage(self, client: mqtt.Client, _, msg: mqtt.MQTTMessage):
        log.debug("%s: %s", msg.topic, msg.payload.decode())
        if msg.topic == "CONTROL" and msg.payload.decode() == "QUIT":
            self.exit = True
