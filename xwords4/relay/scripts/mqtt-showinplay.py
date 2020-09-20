#!/usr/bin/python3

import argparse, re, struct
import paho.mqtt.client as mqtt

g_topics = [
    '$SYS/broker/clients/disconnected',
    # '$SYS/broker/+/+',
    'xw4/device/#',
    ]

sDevIDPat = re.compile('xw4/device/([\dA-F]+)')

# Define event callbacks
def on_connect(client, userdata, flags, rc):
    print('on_connect(): rc: {}'.format(rc))

def on_message(client, obj, msg):
    match = sDevIDPat.match(msg.topic)
    if match:
        (sender, gameID) = getSender(msg.payload)
        print('on_message(): from {} to {}, len: {}'.format(sender, match.group(1), len(msg.payload)))

def on_publish(client, obj, mid):
    print("mid: " + str(mid))

def on_subscribe(client, obj, mid, granted_qos):
    print("Subscribed: " + str(mid) + " " + str(granted_qos))

def on_log(client, obj, level, string):
    print(string)

def getSender(payload):
    result = None
    got = struct.unpack_from('>bqI', payload)
    if 0 == got[0] or 1 == got[0]:
        devID = '{:016X}'.format(got[1])
        gameID = 1 == got[0] and got[2] or 0
        result = (devID, gameID)
    # print('getSender() => {}'.format(result))
    return result

def makeClient():
    mqttc = mqtt.Client()
    # Assign event callbacks
    mqttc.on_message = on_message
    mqttc.on_connect = on_connect
    mqttc.on_publish = on_publish
    mqttc.on_subscribe = on_subscribe
    return mqttc

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', dest = 'HOST', default = 'liquidsugar.net',
                        help = 'the host mosquitto is on')
    parser.add_argument('--port', dest = 'PORT', default = 1883,
                        help = 'the port mosquitto is on')
    return parser

def main():
    args = mkParser().parse_args()

    mqttc = makeClient()
    mqttc.username_pw_set('xwuser', password='xw4r0cks')
    mqttc.connect(args.HOST, args.PORT)
    print('connected to {}:{}'.format(args.HOST, args.PORT))
    # Start subscribe, with QoS level 2
    for topic in g_topics:
        mqttc.subscribe(topic, 2)
    while True:
        err = mqttc.loop()
        if 0 != err:
            print('got {} from loop()'.format(err))
            break

##############################################################################
if __name__ == '__main__':
    main()
