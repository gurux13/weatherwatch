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
  var weather_info_a = text.match(/<div class="today-forecast">([^<]*)</);
  var weather_info = at_or_default(weather_info_a, 1, "Сегодня неизвестно, ветер ? м/с");
  weather_info = weather_info.replace("Сегодня", "");
  var pieces = weather_info.split(",");
  var conditions = "неизвестно";
  if (pieces.length > 0)
    conditions = pieces[0].trim();
  var wind = "? м/с";
  if (pieces.length > 1)
    wind = pieces[1].replace("ветер", "").trim();
  var temperature_a = text.match(/<span class="temp-current i-bem" data-bem="[^"]*">([^<]*)</);
  var temperature = at_or_default(temperature_a, 1, "-273");
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
  var url = "https://p.ya.ru/moscow?lat=" + pos.coords.latitude + "&lon=" + pos.coords.longitude;
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
