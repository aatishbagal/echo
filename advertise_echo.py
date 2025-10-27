#!/usr/bin/env python3
"""
BLE Advertiser for Echo - Makes Echo devices discoverable
Run this alongside the Echo C++ application
"""

import sys
import dbus
import dbus.exceptions
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib
import argparse
import platform

BLUEZ_SERVICE_NAME = 'org.bluez'
LE_ADVERTISING_MANAGER_IFACE = 'org.bluez.LEAdvertisingManager1'
DBUS_OM_IFACE = 'org.freedesktop.DBus.ObjectManager'
DBUS_PROP_IFACE = 'org.freedesktop.DBus.Properties'
LE_ADVERTISEMENT_IFACE = 'org.bluez.LEAdvertisement1'

# BitChat service UUID
ECHO_SERVICE_UUID = 'F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C'

class Advertisement(dbus.service.Object):
    PATH_BASE = '/org/bluez/echo/advertisement'

    def __init__(self, bus, index, advertising_type, peer_id, username, os_type):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.ad_type = advertising_type
        self.service_uuids = [ECHO_SERVICE_UUID]
        self.local_name = f'Echo-{username}[{os_type}]'
        self.include_tx_power = True
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        properties = dict()
        properties['Type'] = self.ad_type
        properties['ServiceUUIDs'] = dbus.Array(self.service_uuids, signature='s')
        properties['LocalName'] = dbus.String(self.local_name)
        properties['IncludeTxPower'] = dbus.Boolean(self.include_tx_power)
        return {LE_ADVERTISEMENT_IFACE: properties}

    def get_path(self):
        return dbus.ObjectPath(self.path)

    @dbus.service.method(DBUS_PROP_IFACE,
                         in_signature='s',
                         out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != LE_ADVERTISEMENT_IFACE:
            raise dbus.exceptions.DBusException(
                'org.freedesktop.DBus.Error.InvalidArgs',
                'Invalid interface')
        return self.get_properties()[LE_ADVERTISEMENT_IFACE]

    @dbus.service.method(LE_ADVERTISEMENT_IFACE,
                         in_signature='',
                         out_signature='')
    def Release(self):
        print(f'{self.path}: Released!')

def find_adapter(bus):
    remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'),
                                DBUS_OM_IFACE)
    objects = remote_om.GetManagedObjects()

    for o, props in objects.items():
        if LE_ADVERTISING_MANAGER_IFACE in props:
            return o

    return None

def main():
    parser = argparse.ArgumentParser(description='Advertise Echo BLE service')
    parser.add_argument('--peer-id', required=True, help='16-character peer ID (first 16 chars of fingerprint)')
    parser.add_argument('--username', default='EchoUser', help='Username')
    args = parser.parse_args()

    os_type = platform.system().lower()
    if os_type == 'darwin':
        os_type = 'macos'

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    bus = dbus.SystemBus()

    adapter = find_adapter(bus)
    if not adapter:
        print('LEAdvertisingManager1 interface not found')
        return

    print(f'Advertising Echo device:')
    print(f'  Peer ID: {args.peer_id}')
    print(f'  Username: {args.username}')
    print(f'  Service UUID: {ECHO_SERVICE_UUID}')
    print(f'  Device Name: Echo-{args.peer_id[:8]}')

    adapter_props = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter),
                                    DBUS_PROP_IFACE)

    adapter_props.Set('org.bluez.Adapter1', 'Powered', dbus.Boolean(True))

    ad_manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter),
                                 LE_ADVERTISING_MANAGER_IFACE)

    # advertisement = Advertisement(bus, 0, 'peripheral', args.peer_id[:8])
    advertisement = Advertisement(bus, 0, 'peripheral', args.peer_id, args.username, os_type)

    mainloop = GLib.MainLoop()

    try:
        ad_manager.RegisterAdvertisement(advertisement.get_path(), {},
                                         reply_handler=lambda: print('Advertisement registered'),
                                         error_handler=lambda error: print(f'Failed to register advertisement: {error}'))
        print('\nPress Ctrl+C to stop advertising...\n')
        mainloop.run()
    except KeyboardInterrupt:
        print('\nStopping advertisement...')
        ad_manager.UnregisterAdvertisement(advertisement.get_path())
        mainloop.quit()

if __name__ == '__main__':
    main()