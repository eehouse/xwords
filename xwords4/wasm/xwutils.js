var state = {client: null,
             closure: null,
			 connected: false,
			};

function ccallString(proc, closure, str) {
	Module.ccall('cbckString', null, ['number', 'number', 'string'],
				 [proc, closure, str]);
}

function registerOnce(devid, gitrev, now) {
	let nextTimeKey = 'next_reg';
	let gitKey = 'last_write';
	let nextTime = parseInt(localStorage.getItem(nextTimeKey));
	let prevGit = localStorage.getItem(gitKey);
	if ( prevGit == gitrev && now < nextTime ) {
		console.log('registerOnce(): next in ' + (nextTime - now) + ' secs');
	} else {
		let args = { devid: devid,
					 gitrev: gitrev,
					 loc: navigator.language,
					 os: navigator.appName,
					 vers: '0.0',
					 dbg: true,
					 myNow: now,
					 vrntName: 'wasm',
				   };
		let body = JSON.stringify(args);

		fetch('/xw4/api/v1/register', {
			method: 'post',
			body: body,
			headers: {
				'Content-Type': 'application/json'
			},
		}).then(res => {
			return res.json();
		}).then(data => {
			console.log('data: ' + JSON.stringify(data));
			if ( data.success ) {
				localStorage.setItem(nextTimeKey, data.atNext);
				localStorage.setItem(gitKey, gitrev);
			}
		});
	}
}

function onHaveDevID(closure, devid, gitrev, now, noTabProc, focusProc) {
	// Set a unique tag so we know if somebody comes along later
	let tabID = Math.random();
	localStorage.setItem('tabID', tabID);
	let listener = function () {
		newTabID = 	localStorage.getItem('tabID');
		if ( newTabID != tabID ) {
			state.client.disconnect();
			ccallString(noTabProc, state.closure, '');
			window.removeEventListener('storage', listener);
		}
	};
	window.addEventListener('storage', listener);

	window.onfocus = function () {
		ccallString(focusProc, state.closure, '');
	};

	registerOnce(devid, gitrev, now);

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

function addDepthNote(dlg) {
	let depth = dlg.parentNode.childElementCount;
	if ( depth > 1 ) {
		let div = document.createElement('div');
		div.textContent = '(Depth: ' + depth + ')';
		dlg.appendChild(div);
	}
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
	for ( let ii = 0; ii < buttons.length; ++ii ) {
		let buttonTxt = buttons[ii];
		let button = document.createElement('button');
		button.textContent = buttonTxt;
		button.onclick = function() { proc(ii); };
		div.appendChild( button );
	}

	return div;
}

function nbDialog(msg, buttons, proc, closure) {
	const dlg = newDlgWMsg( msg );

	const butProc = function(indx) {
		dlg.parentNode.removeChild(dlg);
		ccallString(proc, closure, buttons[indx]);
	}
	dlg.appendChild( newButtonDiv( buttons, butProc ) );
	addDepthNote(dlg);
}

function nbBlankPick(title, buttons, proc, closure) {
	let dlg = newDlgWMsg( title );

	const butProc = function(indx) {
		dlg.parentNode.removeChild(dlg);
		ccallString(proc, closure, indx.toString());
	}

	dlg.appendChild( newButtonDiv( buttons, butProc ) );
	addDepthNote(dlg);
}

// To enable sorting of names on buttons while keeping existing code,
// I'm creating an array of pairs then sorting that.
function nbGamePick(title, gameMap, proc, closure) {
	let dlg = newDlgWMsg( title );

	let pairs = [];
	Object.keys(gameMap).forEach( function(key) {
		pairs.push({id:key, txt:gameMap[key]});
	});

	pairs.sort(function(a, b){
		var stra = a.txt.toLowerCase();
		var strb = b.txt.toLowerCase();
		if (stra < strb) {return -1;}
		if (stra > strb) {return 1;}
		return parseInt(a.id) - parseInt(b.id);
	});
	let buttons = [];
	for ( pair of pairs ) {
		buttons.push(pair.txt);
	}
	
	butProc = function(indx) {
		dlg.parentNode.removeChild(dlg);
		ccallString(proc, closure, pairs[indx].id);
	}

	dlg.appendChild( newButtonDiv( buttons, butProc ) );
	addDepthNote(dlg);
}

function setDivButtons(divid, buttons, proc, closure) {
	let parent = document.getElementById(divid);
	while ( parent.lastElementChild ) {
		parent.removeChild(parent.lastElementChild);
	}

	butProc = function(indx) {
		ccallString(proc, closure, buttons[indx]);
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
		ccallString(proc, closure, str);
	}

	let buttons = ["Cancel", "OK"];
	butProc = function(indx) {
		if ( indx == 0 ) {		// CANCEL
			dismissed(null);
		} else if ( indx == 1 ) { // OK
			dismissed(tarea.value);
		}
	}
	dlg.appendChild( newButtonDiv( buttons, butProc ) );
	addDepthNote(dlg);
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
	const dlg = newDlgWMsg('Is your opponent a robot or someone you will invite?');

	const radioDiv = document.createElement('div');
	dlg.appendChild( radioDiv );
	const robotSet = [null];
	radioDiv.appendChild(newRadio('Robot', 'newgame', function() {robotSet[0] = true;}));
	radioDiv.appendChild(newRadio('Remote player', 'newgame', function() {robotSet[0] = false;}));

	const butProc = function(indx) {
		if ( indx === 1 && null !== robotSet[0]) {
			const types = ['number', 'boolean'];
			const params = [closure, robotSet[0]];
			Module.ccall('onNewGame', null, types, params);
		}
		dlg.parentNode.removeChild(dlg);
	}

	dlg.appendChild( newButtonDiv( ['Cancel', 'OK'], butProc ) );
	addDepthNote(dlg);
}

for ( let one of ['paho-mqtt.js'] ) {
	let script = document.createElement('script');
	script.src = one;
	document.body.appendChild(script);
}
