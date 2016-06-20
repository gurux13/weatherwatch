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
  if (conditions == "облачно с прояснениями")
     conditions = "полуоблачно";
  conditions = conditions.split(" ")[0];
  var wind = "? м/с";
  if (pieces.length > 1)
    wind = pieces[1].replace("ветер", "").trim();
  var temperature_a = text.match(/<span class="temp-current i-bem" data-bem="[^"]*">([^<]*)</);
  var temperature = at_or_default(temperature_a, 1, "-273");
  
  var forecast = "";
  var forecast_re = /"temp-chart__hour">([0-9]*)[ ч]*<\/p>\s*<div class="temp-chart__item temp-chart__item_diff_[^"]*">\s*<div class="temp-chart__temp" data-t="[^"]*">([^<]*)<\/div>\s*<[^>]*>\s*<.{0,40}/g;
  var forecast_a;
  var incl = 0;
  
  while ((forecast_a = forecast_re.exec(text)) !== null) {
    if (incl === 0) {
      var h = forecast_a[1];
      if (h.length == 1)
        h = "0" + h;
      var val = forecast_a[2];
      if (forecast_a[0].indexOf('icon_rain') != -1)
        val += "!";
      forecast += h + " " + val + ", ";
      
    }
    incl = (incl + 1) % 3;
  }
  if (forecast.length > 2)
    forecast = forecast.substring(0, forecast.length - 2);
  console.log("Forecast: " + forecast);
  var rv = {now: {
      conditions: conditions,
      temperature: temperature,
      wind: wind
    }, 
    forecast: forecast
  };
  return rv;
}

function onGotTraffic(text) {
  console.log("Got traffic");
  var matches = text.match(/"level":([0-9]*),/);
  
  var level = at_or_default(matches, 1, "-1");
  matches = text.match(/"icon":"([^"]*)"/);
  
  var icon = at_or_default(matches, 1, "green");
  var toSend = icon.substring(0, 1) + level;
  console.log("Traffic level: " + level + ", icon: " + icon + ", sending: " + toSend);
  //toSend = 'g1';
    var message = {
    'KEY_TRAFFIC': toSend
  };
  Pebble.sendAppMessage(message,
        function(e) {
          console.log("Traffic info sent to Pebble successfully!");
        },
        function(e) {
          console.log("Error sending weather info to Pebble!");
        }
      );

}

function onGotLayers(text) {
  console.log("Got layers.xml");
  var matches = text.match(/"version":"([0-9]*)"/);
  var tm = at_or_default(matches, 1, "-1");
  if (tm == "-1") {
    console.log("Got no version in layers, giving up");
    console.log(text);
    return;
  }
  var urlTraffic = "https://jgo.maps.yandex.net/description/traffic-light?lang=ru_RU&ids=213&tm=" + tm + "&_=9175338";
  
  xhrRequest(urlTraffic, 'GET', onGotTraffic);
}

function locationSuccess(pos) {
  // Construct URL
  console.log("Location acquired, requesting weather");
  var urlWeather = "https://p.ya.ru/moscow?lat=" + pos.coords.latitude + "&lon=" + pos.coords.longitude;
  // Send request to OpenWeatherMap
  xhrRequest(urlWeather, 'GET', 
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
      var forecast = weather.forecast;
      console.log("Conditions are " + conditions);
      console.log("Wind is " + wind);
      
      // Assemble dictionary using our keys
      var dictionary = {
        "KEY_TEMPERATURE": temperature,
        "KEY_CONDITIONS": conditions,
        "KEY_WIND": wind,
        "KEY_FORECAST": forecast
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
  var urlLayers = "https://api-maps.yandex.ru/services/coverage/1.0/layers.xml?lang=ru_RU&l=trf&callback=id_146635635855735192313&_=6404139&host_config%5Bhostname%5D=yandex.ru";
  xhrRequest(urlLayers, 'GET', onGotLayers);
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
    //getWeather();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log("AppMessage received!");
    getWeather();
  }                     
);
