var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.error = function() {
    console.log("XHR ERROR. Status: " + this.readyState);
    console.log("HTTP status: " + xhr.status + ": " + xhr.statusText);

  };

  xhr.open(type, url);
  xhr.send();
  
  
};

function at_or_default(array, id, def) {
  if (array === null || array.length <= id)
    return def;
  return array[id];
}
function extractWeather(text) {
  var conditions_a = text.match(/\"current-weather__comment\">([^<]*)<\//);
  var conditions = at_or_default(conditions_a, 1, "неизвестно");
  if (conditions == "облачно с прояснениями")
    conditions = "полуоблачно";
  conditions = conditions.split(" ")[0];
  var temperature_a = text.match(/([0-9+-]*) °C/);
  var temperature = at_or_default(temperature_a, 1, "-273");
  var wind_a = text.match(/<span class=\"wind-speed\">([^<]*)<\/span>/);
  var wind = at_or_default(wind_a, 1, "? м/с").replace(',','.');
  var rv = {now: {
      conditions: conditions,
      temperature: temperature,
      wind: wind
    }
  };
  return rv;
}

function locationSuccess(pos) {
  // Construct URL
  console.log("Location acquired, requesting weather");
  var url = "https://pogoda.yandex.ru/moscow?lat=" + pos.coords.latitude + "&lon=" + pos.coords.longitude;
  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      console.log("YA response got, length: " + responseText.length);
      // responseText contains a JSON object with weather info
      var weather = extractWeather(responseText);
      console.log("Weather extracted");
      // Temperature in Kelvin requires adjustment
      var temperature = weather.now.temperature;
      console.log("Temperature is " + temperature);

      // Conditions
      var conditions = weather.now.conditions;
      var wind = weather.now.wind;
      console.log("Conditions are " + conditions);
      console.log("Wind is " + wind);
      
      // Assemble dictionary using our keys
      var dictionary = {
        "KEY_TEMPERATURE": temperature,
        "KEY_CONDITIONS": conditions,
        "KEY_WIND": wind
      };

      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          console.log("Weather info sent to Pebble successfully!");
        },
        function(e) {
          console.log("Error sending weather info to Pebble!");
        }
      );
    }      
  );
}

function locationError(err) {
  console.log("Error requesting location!");
}

function getWeather() {
  console.log("Requesting location...");
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 600000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // Get the initial weather
    getWeather();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log("AppMessage received!");
    getWeather();
  }                     
);
