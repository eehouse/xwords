#!/usr/bin/env python3

import json, shelve

"""This will be a prototype of a simple store-and-forward message
passing server. Target clients are peer-to-peer apps like turn-based
games and maybe chat apps. It's expected that they depend on the
server for nothing but message passing and any group-formation that it
depends on (e.g three devices agreeing to participate in a single game
of Fish, a process that gets them a token that can be used to address
messages that are part of that game.) Clients can use this server as
one of several means of communicating, depending on it to deliver
messages e.g. when the devices are out of range of bluetooth.

"""

# register: a device is meant to call this once to get from the server
# an identifier that will identify it from then on. Other APIs will
# require this identifier.
#
# @param clientID: a String the client can optionally provide to link
# this registration to an earlier one. For example, if a client app
# wants state to survive a hard reset of the device but there are IDs
# like serial numbers or a user's email address that will survive that
# process, such an id could be used.
def register(req, clientID = None):
    shelf = openShelf()
    obj = {'deviceID' : shelf['nextID']}
    shelf['nextID'] += 1
    shelf.close()
    return json.dumps(obj)

# Associate attributes with a device that can be used for indirectly
# related purposes. The one I have in mind is GCM (Google Cloud
# Messaging), where the device provides a server an ID that the server
# can use to ask google's servers to forward a push message to the
# device.
def setAttr(req, deviceID, attrKey, attrValue):
    pass

# joinRoom: called when a device wants to start a new game to which
# other devices will also connect. Returns a gameID that internally
# refers to the game joined and the device's position in it.
# @param
def joinRoom(deviceID, room, lang, nTotal, nHere = 1, position = 0):
    pass

def forward(req, deviceID, msg, roomID, positions):
    pass

def openShelf():
    shelf = shelve.open("/tmp/nr.shelf")
    if not 'nextID' in shelf: shelf['nextID'] = 0;
    return shelf

def main():
    pass

##############################################################################
if __name__ == '__main__':
    main()
