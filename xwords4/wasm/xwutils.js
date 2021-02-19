var state = {client: null,
             closure: null,
			 connected: false,
			};

function callNewGame() {
	Module.ccall('newgame', null, ['number'], [state.closure]);
}

function callButton(obj) {
	Module.ccall('button', null, ['number', 'string'], [state.closure, obj.id]);
}

function onHaveDevID(closure, devid) {
	// Set a unique tag so we know if somebody comes along later
	let tabID = Math.random();
	localStorage.setItem('tabID', tabID);
	window.addEventListener('storage', function () {
		newTabID = 	localStorage.getItem('tabID');
		if ( newTabID != tabID ) {
			state.client.disconnect();
			Module.ccall('button', null, ['number', 'string'], [state.closure, 'exit']);
		}
	} );

	state.closure = closure;
	document.getElementById("mqtt_span").textContent=devid;

	function tellConnected(isConn) {
		Module.ccall('MQTTConnectedChanged', null, ['number', 'boolean'], [state.closure, isConn]);
	}

	state.client = new Paho.MQTT.Client("eehouse.org", 8883, '/wss', devid);

	// set callback handlers
	state.client.onConnectionLost = function onConnectionLost(responseObject) {
		state.connected = false;
		document.getElementById("mqtt_status").textContent="Disconnected";
		tellConnected(false);
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
		tellConnected(true);

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

function newDlgWMsg(msg) {
	let container = document.getElementById('nbalert');

	let dlg = document.createElement('div');
	dlg.classList.add('nbalert');
	dlg.style.zIndex = 10000 + container.childElementCount;
	container.appendChild( dlg );

	let txtDiv = document.createElement('div');
	txtDiv.textContent = msg
	dlg.appendChild( txtDiv );

	return dlg;
}

function newButtonDiv(buttons, proc) {
	let div = document.createElement('div');
	div.classList.add('buttonRow');
	for ( let buttonTxt of buttons ) {
		let button = document.createElement('button');
		button.textContent = buttonTxt;
		button.onclick = function() { proc(buttonTxt); };
		div.appendChild( button );
	}

	return div;
}

function nbDialog(msg, buttons, proc, closure) {
	let dlg = newDlgWMsg( msg );

	butProc = function(buttonTxt) {
		Module.ccall('onDlgButton', null, ['number', 'number', 'string'],
					 [proc, closure, buttonTxt]);
		dlg.parentNode.removeChild(dlg);
	}
	dlg.appendChild( newButtonDiv( buttons, butProc ) );
}

function nbBlankPick(title, buttons, proc, closure) {
	let dlg = newDlgWMsg( title );

	butProc = function(buttonTxt) {
		Module.ccall('onDlgButton', null, ['number', 'number', 'string'],
					 [proc, closure, buttonTxt]);
		dlg.parentNode.removeChild(dlg);
	}

	let ROWLEN = 6;
	for (ii = 0; ii < buttons.length; ii += ROWLEN) {
		dlg.appendChild( newButtonDiv( buttons.slice(ii, ii + ROWLEN), butProc ) );
	}
}

function nbGamePick(title, gameMap, proc, closure) {
	let dlg = newDlgWMsg( title );

	buttons = [];
	revMap = {}
	Object.keys(gameMap).forEach( function(key) {
		let val = gameMap[key]
		buttons.push(val);
		revMap[val] = key;
	});
	
	butProc = function(buttonTxt) {
		Module.ccall('onDlgButton', null, ['number', 'number', 'string'],
		 			 [proc, closure, revMap[buttonTxt]]);
		dlg.parentNode.removeChild(dlg);
	}

	dlg.appendChild( newButtonDiv( buttons, butProc ) );
}

function setDivButtons(divid, buttons, proc, closure) {
	let parent = document.getElementById(divid);
	while ( parent.lastElementChild ) {
		parent.removeChild(parent.lastElementChild);
	}

	butProc = function(buttonTxt) {
		Module.ccall('onDlgButton', null, ['number', 'number', 'string'],
					 [proc, closure, buttonTxt]);
	}

	parent.appendChild( newButtonDiv( buttons, butProc ) );
}

function nbGetString(msg, dflt, proc, closure) {
	let dlg = newDlgWMsg( msg );

	let tarea = document.createElement('textarea');
	tarea.classList.add('stringedit');
	tarea.value = dflt;
	dlg.appendChild( tarea );

	dismissed = function(str) {
		dlg.parentNode.removeChild(dlg);
		Module.ccall('onDlgButton', null, ['number', 'number', 'string'],
					 [proc, closure, str]);
	}

	buttons = ["Cancel", "OK"];
	butProc = function(str) {
		if ( str == buttons[0] ) {
			dismissed(null);
		} else if ( str == buttons[1] ) {
			dismissed(tarea.value);
		}
	}
	dlg.appendChild( newButtonDiv( buttons, butProc ) );
}

function newRadio(txt, id, proc) {
	let span = document.createElement('span');
	let radio = document.createElement('input');
	radio.type = 'radio';
	radio.id = id;
	radio.name = id;
	radio.onclick = proc;

	let label = document.createElement('label')
	label.htmlFor = id;
	var description = document.createTextNode(txt);
	label.appendChild(description);

	span.appendChild(label);
	span.appendChild(radio);

	return span;
}

function nbGetNewGame(closure, msg) {
	let dlg = newDlgWMsg('Is your opponent a robot or someone you will invite?');

	let radioDiv = document.createElement('div');
	dlg.appendChild( radioDiv );
	var robotSet = [null];
	radioDiv.appendChild(newRadio('Robot', 'newgame', function() {robotSet[0] = true;}));
	radioDiv.appendChild(newRadio('Remote player', 'newgame', function() {robotSet[0] = false;}));

	butProc = function(str) {
		if ( str === 'OK' && null !== robotSet[0]) {
			types = ['number', 'boolean'];
			params = [closure, robotSet[0]];
			Module.ccall('onNewGame', null, types, params);
		}
		dlg.parentNode.removeChild(dlg);
	}

	dlg.appendChild( newButtonDiv( ['Cancel', 'OK'], butProc ) );
}

for ( let one of ['paho-mqtt.js'] ) {
	let script = document.createElement('script');
	script.src = one;
	document.body.appendChild(script);
}
