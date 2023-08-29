#!.venv/bin/python3

import logging
from collections.abc import Callable, ValuesView
from time import sleep

from bluepy.btle import (
    UUID,
    BTLEException,
    Characteristic,
    DefaultDelegate,
    Peripheral,
    ScanEntry,
    Scanner,
    Service
)

from mqtt import MqttHandler
from logger import Logger
from config import device_name

log = Logger(__name__)


class ScannerDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        if isNewDev:
            log.info("Discovered device %s (%s)",
                     dev.getValueText(9), dev.addr)
        if isNewData:
            log.info("Received new data from device %s", dev.addr)


class Bluetooth:
    envSensService = UUID(0x181a)
    scanner = Scanner().withDelegate(ScannerDelegate())
    charData: bytes | None = None

    def process(self):
        devices = self.scanner.scan(8)
        self.handleDevices(devices)

    def readServices(self, conn: Peripheral):
        try:
            service: Service = conn.getServiceByUUID(self.envSensService)
            chars: list[Characteristic] = service.getCharacteristics()
            for char in chars:
                if char.supportsRead():
                    self.charData = char.read()
                    log.info(f"{self.charData}")
                else:
                    log.error("Can't read characteristic")
        except BTLEException:
            log.error("Service not found %s",
                      self.envSensService.getCommonName())

    @staticmethod
    def formatMsg(dev: ScanEntry):
        msg = ("Device %s (%s), RSSI=%d dB\n" %
               (dev.addr, dev.addrType, dev.rssi))
        for (adType, _, value) in dev.getScanData():
            msg += ("adType: %d, value: %s\n" % (adType, value))
        return msg.rstrip()

    def infoLogDeviceInfo(self, dev: ScanEntry):
        log.info(self.formatMsg(dev))

    def debugLogDeviceInfo(self, dev: ScanEntry):
        log.debug(self.formatMsg(dev))

    def handleDevices(self, devices: ValuesView[ScanEntry]):
        self.charData = None
        for dev in devices:
            self.debugLogDeviceInfo(dev)
            for (adType, _, value) in dev.getScanData():
                if adType == 9 and value == device_name:
                    self.infoLogDeviceInfo(dev)
                    try:
                        with Peripheral(dev) as conn:
                            self.readServices(conn)
                    except BTLEException:
                        log.error("Connection failed", exc_info=True)


if __name__ == "__main__":
    handler = Bluetooth()
    try:
        with MqttHandler() as mqtt:
            log.info("Start main")
            log.info(f"Looking for {device_name}")
            while not mqtt.exit:
                handler.process()
                if handler.charData:
                    info = mqtt.client.publish("TEST", handler.charData)
                    log.info(handler.charData)
                sleep(1)
    finally:
        log.info("Start main")
        log.info(f"Looking for {device_name}")
        while True:
            handler.process()
            if handler.charData:
                log.info(handler.charData)
            sleep(1)
