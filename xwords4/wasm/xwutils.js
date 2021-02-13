var state = {client: null,
             closure: null,
			 connected: false,
			};

function callNewGame() {
	var args = [ state.closure,
				 document.getElementById("player0Checked").checked,
				 document.getElementById("player1Checked").checked,
			   ];
	Module.ccall('newgame', null, ['number', 'boolean', 'boolean'], args);
}

function callButton(obj) {
	Module.ccall('button', null, ['number', 'string'], [state.closure, obj.id]);
}

function onHaveDevID(closure, devid) {
    console.log('got ' + devid);
	state.closure = closure;
	document.getElementById("mqtt_span").textContent=devid;

	state.client = new Paho.MQTT.Client("eehouse.org", 8883, '/wss', devid);

	// set callback handlers
	state.client.onConnectionLost = function onConnectionLost(responseObject) {
		state.connected = false;
		document.getElementById("mqtt_status").textContent="Disconnected";
		if (responseObject.errorCode !== 0) {
			console.log("onConnectionLost:"+responseObject.errorMessage);
		}
	};
	state.client.onMessageArrived = function onMessageArrived(message) {
		var payload = message.payloadBytes;
		var length = payload.length;
		Module.ccall('gotMQTTMsg', null, ['number', 'number', 'array'],
					 [state.closure, length, payload]);
	};

	function onConnect() {
		state.connected = true
		document.getElementById("mqtt_status").textContent="Connected";

		var subscribeOptions = {
			qos: 2,  // QoS
			// invocationContext: {foo: true},  // Passed to success / failure callback
			// onSuccess: function() { alert('subscribe succeeded'); },
			onFailure: function() { alert('subscribe failed'); },
			timeout: 10,
		};
		state.client.subscribe('xw4/device/' + devid, subscribeOptions);
	}

	state.client.connect({mqttVersion: 3,
						  userName: "xwuser",
						  password: "xw4r0cks",
						  useSSL: true,
						  reconnect: true,
						  onSuccess: onConnect,
						  onFailure: function() { alert('onFailure'); },
						 });

}

function mqttSend( topic, ptr ) {
	let canSend = null != state.client && state.connected;
	if ( canSend ) {
		message = new Paho.MQTT.Message(ptr);
		message.destinationName = topic;
		message.qos = 2;
		state.client.send(message);
	} else {
		console.log('mqttSend: not connected');
	}
	return canSend;
}

function nbDialog(msg, buttons, proc, closure) {
	let dlg = document.getElementById('nbalert');
	while (dlg.firstChild) {
        dlg.removeChild(dlg.firstChild);
    }

	let txtDiv = document.createElement('div');
	txtDiv.textContent = msg
	dlg.appendChild( txtDiv );

	let span = document.createElement('div');
	for ( let buttonTxt of buttons ) {
		let button = document.createElement('button');
		button.textContent = buttonTxt;
		button.onclick = function() {
			Module.ccall('onDlgButton', null, ['number', 'number', 'string'],
						 [proc, closure, buttonTxt]);
			dlg.style.display = 'none'; // hide
		};
		span.appendChild( button );
	}
	dlg.appendChild( span );

	dlg.style.display = 'block'; // reveal
}

for ( let one of ['paho-mqtt.js'] ) {
	let script = document.createElement('script');
	script.src = one
	document.body.appendChild(script);
}
