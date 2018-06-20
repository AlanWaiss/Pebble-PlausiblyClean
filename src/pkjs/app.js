/// <reference path="_weather-interface.js" />
var keys = require('message_keys');
var _location,
	_tries = 0;

function xhrRequest(url, type, callback, context) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function() {
		_tries = 0;
		delete getWeather.to;
		callback.call(context || this, this.responseText);
	};
	xhr.timeout = 2000;
	xhr.ontimeout = function() {
		if(_tries++ < 3)
			getWeather.to = setTimeout(getWeather, 5000 * _tries);
		else {
			var message = {};
			message[keys.CONDITIONS] = "Squirrels got into the weather satellites.";
			Pebble.sendAppMessage(message, messageSuccess, messageError);
		}
	};
	xhr.open(type, url);
	xhr.send();
}

function messageSuccess(e) {
	//console.log('Weather info sent to Pebble successfully!');
}
function messageError(e) {
	console.log('Error sending weather info to Pebble!');
}

function locationSuccess(pos) {
	_location = pos.coords;
	//console.log(JSON.stringify(_location));
	_tries = 0;
	getWeather();
}

function locationError(err) {
	console.log('Error requesting location!');
}

function getLocation() {
	//console.log('Getting current position...');
	navigator.geolocation.getCurrentPosition(
		  locationSuccess, locationError,
	  { timeout: 15000, maximumAge: 300000 }//5 minutes
	);
}

function parseColor(hex) {
	return parseInt(hex.replace(/^\#/, "0x")) || 0;
}

function loadConfig() {
	var config = localStorage.config;
	if(config) {
		//console.log("loadConfig", config);
		config = JSON.parse(config);
		var message = {};
		message[keys.COLOR_BG] = parseColor(config.bg || "#000000");
		message[keys.COLOR_TIME] = parseColor(config.time || "#FFFFFF");
		message[keys.COLOR_TEMP] = parseColor(config.temp || "#00FFFF");
		message[keys.COLOR_DAY] = parseColor(config.day || "#FFAA00");
		message[keys.COLOR_DATE] = parseColor(config.date || "#FFFFFF");
		message[keys.COLOR_AMPM] = parseColor(config.ampm || "#FFAA00");
		message[keys.COLOR_WEATHER] = parseColor(config.weather || "#FFAA00");
		message[keys.COLOR_BATTERY] = parseColor(config.battery || "#000000");
		Pebble.sendAppMessage(message);
	}
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', function(e) {
	//console.log('Getting weather...');
	// Get the initial location
	loadConfig();
	var message = localStorage.weather;
	if(message)
		Pebble.sendAppMessage(JSON.parse(message), messageSuccess, messageError);
	getLocation();
});

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage', function(e) {
	//console.log('AppMessage received!');
	if(getWeather.to)
		clearTimeout(getWeather.to);
	if(!_location || (e.payload && e.payload["50"]))
		getLocation();
	else
		getWeather();
});

Pebble.addEventListener('showConfiguration', function(e) {
	// Show config page
	Pebble.openURL('http://public.dev.dsi.data-systems.com/alan/pebble/config.html');
});

Pebble.addEventListener('webviewclosed', function(e) {
	var config = decodeURIComponent(e.response);
	//console.log('Configuration window returned: ' + config);
	localStorage.config = config;
	loadConfig();
});
/// <reference path="_base.js" />
var _icon = {
	"clear-day": 1,
	"clear-night": 2,
	"partly-cloudy-day": 3,
	"partly-cloudy-night": 4,
	cloudy: 5,
	rain: 6,
	snow: 7,
	sleet: 8,
	wind: 9,
	fog: 10
};
function getWeather() {
	// We will request the weather here
	//console.log("getWeather");
	var url = "https://api.darksky.net/forecast/a5f7eaee6f997519a920c1c3fcf9117e/" + _location.latitude + "," + _location.longitude;
	xhrRequest(url, "GET", weatherResult);
}

function weatherResult(responseText) {
	// responseText contains a JSON object with weather info
	var json = JSON.parse(responseText);
	var current = json.currently;
	var today = json.daily.data[0];
	var hourly = json.hourly.data;
	var find = new Date();
	find.setMinutes(find.getMinutes() + 75);
	//console.log(responseText);

	var message = {};
	message[keys.TEMPERATURE] = Math.round(current.temperature);
	message[keys.CONDITIONS] = (((new Date().getHours() >= 20 && json.daily.data[0]) || today).summary || current.summary).substr(0, 60);	//after 8:00 PM, use tomorrow's summary
		//+ ", " + Math.round(json.daily.data[0].temperatureMin) + "-" + Math.round(json.daily.data[0].temperatureMax) + "°";
	message[keys.ICON] = _icon[current.icon] || 0;
	for(var i = 0, len = hourly.length; i < len; i++) {
		if(new Date(hourly[i].time * 1000) > find) {
			message[keys.NEXT_TEMP] = Math.round(hourly[i].temperature);
			//console.log(JSON.stringify(hourly[i]));
			break;
		}
	}
	localStorage.weather = JSON.stringify(message);
	//console.log("Sending weather");
	Pebble.sendAppMessage(message, messageSuccess, messageError);
}