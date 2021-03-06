var xhrRequest = function (url, type, callback) {
    var xhr = new XMLHttpRequest();
    
    xhr.onload = function () {
        callback(this.responseText);
    };
  
    xhr.error = function () {
        console.log("XHR ERROR. Status: " + this.readyState);
        console.log("HTTP status: " + xhr.status + ": " + xhr.statusText);

    };

    xhr.open(type, url);
    xhr.send();


};
var city = "";
function in_array(needle, haystack, strict) {	// Checks if a value exists in an array
    // 
    // +   original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)

    var found = false, key;
    strict = !!strict;

    for (key in haystack) {
        if ((strict && haystack[key] === needle) || (!strict && haystack[key] == needle)) {
            found = true;
            break;
        }
    }

    return found;
}
function at_or_default(array, id, def) {
    if (array === null || array.length <= id)
        return def;
    return array[id];
}
function onCity() {
    console.log("City obtained: " + city);
}
function getCity(lat, lng) {

    var url = "http://maps.googleapis.com/maps/api/geocode/json?latlng=" + lat + "," + lng;
    console.log("Requesting city at url " + url);
    xhrRequest(url, 'GET', function (text) {
        //console.log("Got city info: " + text);
        //I hate parsing and checking json...
        var json = JSON.parse(text);
        if (json.results !== undefined) {
            console.log("Have city results");
            for (var i = 0; i < json.results.length; ++i) {
                var result = json.results[i];
                console.log("This result types: " + result.types);
                if (result.types !== undefined && in_array('locality', result.types, true)) {
                    console.log("Found locality result");
                    if (result.address_components !== undefined) {
                        for (var j = 0; j < result.address_components.length; ++j) {
                            var component = result.address_components[j];
                            console.log("This component types: " + component.types);
                            if (component.types !== undefined && in_array('locality', component.types, true)) {
                                console.log("Found locality address component");
                                if (component.short_name !== undefined) {
                                    city = component.short_name;
                                    onCity();
                                    return;
                                }
                                if (component.long_name !== undefined) {
                                    city = component.long_name;
                                    onCity();
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    });
}

function getIcon(ya_name) {
  console.log('Icon: ' + ya_name);
  
  if (ya_name && ya_name.length > 0 && ya_name[0].length > 1) {
    console.log("Array detected");
    var rv = '';
    for (var i = 0; i < ya_name.length; ++i)
      rv += getIcon(ya_name[i]);
    return rv;
  }
  if (ya_name.match(/_bkn/))
    return 'p';
  if (ya_name.match(/-ts/))
    return 't';
  if (ya_name.match(/-sn/))
    return 's';
  
  if (ya_name.match(/-ra/))
    return 'r';
  if (ya_name.match(/_ovc/))
    return 'c';
  if (ya_name.match(/_skc/))
    return 'S';
  return 'u';
}
function makeTemp(val) {
  if (val.startsWith('+') || val.startsWith('-'))
    return val;
  return '+' + val;
}
function extractDailyFcast2(text) {
  var matches = text.match(/<dt class="brief__day[" ]((?!<\/dl>).)*<\/dl>/g);
  //console.log("FACST text:" + text);
  var retval = "";
  var have = 0;
  var nowIcon = [];
  if (matches) {
    for (var idx = 0; idx < matches.length; ++idx) { //skip today
      //console.log(matches[idx]);
      var m = matches[idx].replace('−', '-');
      var date = at_or_default(m.match(/id="d_([0-9]+)/), 1, '');
      var tDay = at_or_default(m.match(/днём[^°]*>([-+0-9]+)°/), 1, '');
      var tNight = at_or_default(m.match(/ночью[^°]*>([-+0-9]+)°/), 1, '');
      var icon = 'unk';
      if (idx === 0) {
        var iconMatches = m.match(/(icon_thumb[-a-zA-Z_0-9]*)/g);
        for (var i = 0; i < iconMatches.length; ++i) {
          console.log("Daily icon:" + iconMatches[i]);
          nowIcon = nowIcon.concat([iconMatches[i]]);
        }
        //icon = at_or_default(m.match(/>сегодня((?!<i).)*<i class="icon [^"]*(thumb[^ "]*)/), 2, 'unk');
        console.log("Current icon: " + nowIcon);
      }
      else 
        icon = at_or_default(m.match(/днём((?!<i).)*<i class="icon [^"]* (icon_thumb[^ "]*)/), 2, 'unk');
      if (idx === 0) { //today
        //nowIcon = icon;
        continue;
      }
      console.log("Icon: " + icon);
      if (!date || !tDay || !tNight || !icon) {
        console.log("Skipped: " + m);
        console.log("Date " + date + ", tDay " + tDay + " tNight" + tNight + " icon " + icon);
        continue;
      }
      retval += date + "$" + makeTemp(tDay) + "/" + makeTemp(tNight) + "$" + getIcon(icon) + "|";
      ++have;
      if (have == 3)
        break;
    }
    
  }
  return {
    forecast:retval,
    nowIcon: nowIcon
  };
}

function extractExtendedWeather(text) {
    //var pressure_info_a = text.match(/<div class="current-weather__condition-item">Давление: ([0-9]*) мм рт. ст./);
  //console.log("Extended weather text: " + text);
    var pressure_info_a = text.match(/([0-9]+) <[^>]*>мм рт\. ст\./);

    var pressure_info = at_or_default(pressure_info_a, 1, "888");

    var matches = text.match(/<li class="forecast-brief__item[" ]((?!<\/li>).)*<\/li>/g);
    var retval = "";
    var have = 0;
    if (matches) {
      console.log("Will use v1 forecast");
        for (var idx = 1; idx < matches.length; ++idx) { //skip today
            //console.log(matches[idx]);
            var m = matches[idx].replace('−', '-');
            var date = at_or_default(m.match(/<span class="forecast-brief__item-day">([0-9]*)/), 1, '');
            var tDay = at_or_default(m.match(/[ "]forecast-brief__item-temp-day[" ][^>]*>\s*([-+0-9]*)/), 1, '');
            var tNight = at_or_default(m.match(/[ "]forecast-brief__item-temp-night[" ][^>]*>\s*([-+0-9]*)/), 1, '');
            var icon = at_or_default(m.match(/<i class="icon ([^ "]*)/), 1, 'unk');
            console.log("Icon: " + icon);
            if (!date || !tDay || !tNight || !icon) {
              console.log("Skipped: " + m);
                console.log("Date " + date + ", tDay " + tDay + " tNight" + tNight + " icon " + icon);
                continue;
            }
          retval += date + "$" + tDay + "/" + tNight + "$" + getIcon(icon) + "|";
          ++have;
          if (have == 3)
            break;
        }
      retval = {
        forecast:retval,
        nowIcon:'unknown'
      };
    } else {
      console.log("Will use v2 forecast");
      retval = extractDailyFcast2(text);
    }
    console.log("Forecast:" + retval.forecast);

    
    return {
        pressure: pressure_info,
        forecast: retval.forecast,
        nowIcon: getIcon(retval.nowIcon)
    };

}

function extractWeather(text) {
    var weather_info_a = text.match(/<div class="today-forecast">([^<]*)</);
    var weather_info = at_or_default(weather_info_a, 1, "Сегодня неизвестно, ветер ? м/с");
    weather_info = weather_info.replace("Сегодня", "");
    console.log(weather_info);
    var pieces = weather_info.split(",");
    var conditions = "неизвестно";
    if (pieces.length > 0)
        conditions = pieces[0].trim();
    if (conditions == "облачно с прояснениями" || conditions == 'малооблачно')
        conditions = "1/2облачно";
    conditions = conditions.replace("небольшой ", "м.");
    conditions = conditions.split(" ")[0];
    var wind = "? м/с";
    if (pieces.length > 1)
        wind = pieces[1].replace("ветер", "").trim();
    var temperature_a = text.match(/<span class="temp-current i-bem" data-bem='[^']*'>([^<]*)</);
    var temperature = at_or_default(temperature_a, 1, "-273");

    var forecast = "";
    var forecast_re = /"temp-chart__hour"[^>]*>([0-9]*)[ ч]*<\/p>\s*<div class="temp-chart__item[^"]*"[^>]*>\s*<div class="temp-chart__temp" data-t="[^"]*">([^<]*)<\/div>\s*<[^>]*>\s*<.{0,40}/g;
    var forecast_a;
    var incl = 0;

    while ((forecast_a = forecast_re.exec(text)) !== null) {
        if (incl === 0) {
            var h = forecast_a[1];
            if (h.length == 1)
                h = "0" + h;
            var val = forecast_a[2];
            if (val == '0')
                val = "+0";

            if (forecast_a[0].indexOf('icon_rain') != -1)
                val += "!";
            if (forecast_a[0].indexOf('icon_snow') != -1)
                val += "*";

            forecast += h + val + ", ";

        }
        incl = (incl + 1) % 3;
    }
    if (forecast.length > 2)
        forecast = forecast.substring(0, forecast.length - 2);
    console.log("Forecast: " + forecast);
    var rv = {
        now: {
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
  //console.log(text);
    var matches = text.match(/"level":([0-9]*),/);

    var level = at_or_default(matches, 1, "-1");
    matches = text.match(/"icon":"([^"]*)"/);

    var icon = at_or_default(matches, 1, "green");
    var toSend = icon.substring(0, 1) + level;
    console.log("Traffic level: " + level + ", icon: " + icon + ", sending: " + toSend);
    //toSend = 'y5';
    var message = {
        'KEY_TRAFFIC': toSend
    };
    Pebble.sendAppMessage(message,
          function (e) {
              console.log("Traffic info sent to Pebble successfully!");
          },
          function (e) {
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
var hadLocationError = false;
var locationRequested = false;
function getShortUrl(html, lat, lng) {
    //href="//p.ya.ru/tver">Почасовой прогноз</a>
  //console.log(html);
    var match = html.match(/\shref="\/\/([^"]*)">Почасовой прогноз<\/a>/);
    var url = at_or_default(match, 1, '');
    if (url === '')
    {
      match = html.match(/link rel="canonical" href="\/\/yandex.ru\/pogoda\/([^"]*)"\/>/);
      var city = at_or_default(match, 1, "moscow");
      console.log("founD ya city " + city);
      url = "p.ya.ru/"+city;
    } 
    
    console.log(url);
    return "https://" + url + "?lat=" + lat + "&lng=" + lng;
}
function locationSuccess(pos) {
    // Construct URL
    hadLocationError = false;
    locationRequested = false;
    console.log("Location acquired, requesting weather");
    getCity(pos.coords.latitude, pos.coords.longitude);
    var urlWeather = "https://yandex.ru/pogoda/moscow/?lat=" + pos.coords.latitude + "&lon=" + pos.coords.longitude;
    //var urlWeather = "https://p.ya.ru/moscow?lat=" + pos.coords.latitude + "&lon=" + pos.coords.longitude;
    console.log("Requesting weather at " + urlWeather);
    // Send request to OpenWeatherMap
    xhrRequest(urlWeather, 'GET',
      function (responseTextExtended) {
          console.log("Got big YA response");
        //console.log(responseTextExtended);
          var shortUrl = getShortUrl(responseTextExtended);
          xhrRequest(shortUrl, 'GET',
          function (responseText) {

              console.log("YA short response got, length: " + responseText.length);
              // responseText contains a JSON object with weather info
              var weather = extractWeather(responseText);
              var extendedWeather = extractExtendedWeather(responseTextExtended);
              console.log("Weather extracted");
              var pressure = extendedWeather.pressure;
              // Temperature in Kelvin requires adjustment
              var temperature = weather.now.temperature;
              console.log("Temperature is " + temperature);
              if (pos.coords.fallback !== undefined) {
                  temperature = "? " + temperature;
              }
              // Conditions
              var conditions = extendedWeather.nowIcon;// weather.now.conditions;
              conditions += "uuu"; //just in case - we need 3 chars.
              var wind = weather.now.wind;
              var forecast = weather.forecast + "|" + extendedWeather.forecast;
              console.log("Conditions are " + conditions);
              console.log("Wind is " + wind);
              var weatherString = temperature + ", " + wind + ", " + pressure + "\n" + conditions;
              // Assemble dictionary using our keys
              var dictionary = {
                  "KEY_WEATHER": weatherString,
                  "KEY_FORECAST": forecast,
                  "KEY_PRESSURE": parseInt(pressure)
              };

              // Send to Pebble
              Pebble.sendAppMessage(dictionary,
                function (e) {
                    console.log("Weather info sent to Pebble successfully!");
                },
                function (e) {
                    console.log("Error sending weather info to Pebble!");
                }
              );

          });
      }
    );
    var urlLayers = "https://api-maps.yandex.ru/services/coverage/1.0/layers.xml?lang=ru_RU&l=trf&callback=id_146635635855735192313&_=6404139&host_config%5Bhostname%5D=yandex.ru";
    xhrRequest(urlLayers, 'GET', onGotLayers);
}
function requestLocation(cached) {
    var options = { timeout: 60000, maximumAge: 3600000, enableHighAccuracy: true };
    if (cached)
        options.maximumAge = Infinity;
    navigator.geolocation.getCurrentPosition(
      locationSuccess,
      locationError,
      options
    );
}
function locationError(err) {
    console.log("Error requesting location: " + err.message);
    console.log(err);
    locationRequested = false;
    if (hadLocationError) {
        console.log("Double fault: defaulting to MSK");
        locationSuccess({
            coords: {
                latitude: '55.675500',
                longitude: '37.446291',
                fallback: true
            }
        });
        return; //Do not make infinite loops;
    }
    console.log("Trying old location.");
    hadLocationError = true;
    if (locationRequested) {
        console.log("No re-requesting location");
        return;
    }
    locationRequested = true;
    requestLocation(true);
}
function parseCurrency(text) {
    text = text.replace(/.*value"\s*:\s*/, '').replace(/[^0-9.]/g, '');
    var val = parseFloat(text).toFixed(2);
    return val;
}
var eur = "";
var usd = "";
function send_currencies_if_both() {
    console.log('Got a currency. Now eur = ' + eur + ", usd = " + usd);
    if (eur && usd) {
        var toSend = usd + "  " + eur;
        var message = {
            'KEY_CURRENCY': toSend
        };
        Pebble.sendAppMessage(message,
              function (e) {
                  console.log("Currencies info sent to Pebble successfully!");
              },
              function (e) {
                  console.log("Error sending weather info to Pebble!");
              }
        );
        eur = usd = "";
    }

}
function onEur(text) {
    eur = '€' + parseCurrency(text);

    send_currencies_if_both();
}
function onUsd(text) {
    usd = '$' + parseCurrency(text);
    send_currencies_if_both();
}
function getWeather() {
    console.log("Requesting location...");
    if (locationRequested) {
        console.log("No re-requesting location");
        return;
    }
    locationRequested = true;

    requestLocation(false);
}
function getCurrencies() {
    var usdUrl = "http://www.rbc.ru/money_graph/latest/59111/";
    xhrRequest(usdUrl, 'GET', onUsd);
    var eurUrl = "http://www.rbc.ru/money_graph/latest/59090/";
    xhrRequest(eurUrl, 'GET', onEur);

}
function getAll() {
    getWeather();
    getCurrencies();
}
// Listen for when the watchface is opened
Pebble.addEventListener('ready',
  function (e) {
      console.log("PebbleKit JS ready!");

      // Get the initial weather
      //getWeather();
      //Only currencies are free
      getCurrencies();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function (e) {
      console.log("AppMessage received!");
      getAll();
  }
);
