#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import json


TEMP_UMBRAL = 38
HUM_UMBRAL = 80
LUX_UMBRAL = 200
VIBR_UMBRAL = 8

sensores = ["TEMP","HUM","LUX","VIBR"]
sensores_umbrales = [TEMP_UMBRAL,HUM_UMBRAL,LUX_UMBRAL,VIBR_UMBRAL]


# Callback ON_CONNECT
def on_connect(client, userdata, flags, rc):
	topic_edificio = "/EDIFICIO_8/#"
	client.subscribe(topic_edificio)
	print("Suscrito a todo al edificio %s" % topic_edificio)

# Callback ON_MESSAGE
def on_message(client,userdata,msg):
	datos = json.loads(msg.payload)
	
	for i in range(len(sensores)):
		if datos[sensores[i]] >  sensores_umbrales[i]:
			info = msg.topic.split('/')
			print("Edificio\t" + info[1])
			print("Planta\t\t" + info[2])
			print("Ala\t\t" + info[3])
			print("Sala\t\t" + info[4])
			print("Sensor %s\t%s\n" % (sensores[i],datos[sensores[i]]))

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("127.0.0.1",1883)

client.loop_forever()

