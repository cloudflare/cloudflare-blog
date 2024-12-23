from js import console, Response, Request, fetch
import json
from datetime import datetime
import pyratemp
import logging
import time

logger = logging.getLogger(__name__)

temp = '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>eInk Display</title>
    <style>
        body {
            font-smooth: never;
            margin: 0;
            padding: 0;
            font-family: sans-serif;
            font-weight: bold;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-color: white;
            overflow: hidden; /* Hide scrollbars */
        }
        .container {
            width: 480px;
            height: 800px;
            display: flex;
            flex-direction: column;
            justify-content: space-around;
            border: 0px solid;
        }
        .topdate {
            text-align:center;
            font-size: 2em;
        }
        .topdawndusk {
            text-align:center;
            font-size: 1em;
            margin-bottom:20px;
        }
        .date {
            margin-left: 20px;
            font-size: 1.5em;
            font-weight: bold;
        }
        .weather {
            margin-left: 40px;
            display: flex;
            align-items:center;
            text-align: left;
            font-size: 3em;
        }
        .temperature {
            font-size: 3em;
        }
        .events {
            font-size: 1.5em;
        }
    </style>
</head>
<body>
<div class="container">
  <div class="topdate">@!date!@
<!--(if w["stale"])-->
  (stale @!w["stale"]!@)
<!--(end)-->
  </div>
  <div class="topdawndusk">☀︎@!dusk!@ - ⏾@!dawn!@</div>

<!--(for step in hour_steps)-->
<div>
    <div>
        <span class="date"><B>@!w["hourly"]["time"][step][-5:]!@</B></span>
        &nbsp;&nbsp;&nbsp;
        <span class="temperature">
            @!round(w["hourly"]["apparent_temperature"][step])!@°C
        </span>
        &nbsp;&nbsp;
        <span>wind @!round(w["hourly"]["wind_speed_10m"][step])!@<small><sup>km</sup>/<sub>h</sub></small> </span>
        &nbsp;&nbsp;
        <span>
        rain @!round(w["hourly"]["precipitation_probability"][step])!@%
        </span>
        </div>
        <div class="weather">
          <span>@!w["hourly"]["wmo_icon"][step]!@</span>&nbsp;
          <span><small>@!w["hourly"]["wmo_text"][step]!@</small></span>
        </div>
    <hr/>
    </div>
<!--(end)-->

</div>
'''

wmo_icons = {
  0: "☀️", # Clear sky
  1: "🌤️", # Mainly clear
  2: "⛅", # Partly cloudy
  3: "☁️", # Overcast
  45: "🌫️", # Fog
  48: "🌫️", # Depositing rime fog
  51: "🌧️", # Drizzle: Light intensity
  53: "🌧️", # Drizzle: Moderate intensity
  55: "🌧️", # Drizzle: Dense intensity
  56: "🌧️", # Freezing Drizzle: Light intensity
  57: "🌧️", # Freezing Drizzle: Dense intensity
  61: "🌧️", # Rain: Slight intensity
  63: "🌧️", # Rain: Moderate intensity
  65: "🌧️", # Rain: Heavy intensity
  66: "🌧️", # Freezing Rain: Light intensity
  67: "🌧️", # Freezing Rain: Heavy intensity
  71: "❄️", # Snow fall: Slight intensity
  73: "❄️", # Snow fall: Moderate intensity
  75: "❄️", # Snow fall: Heavy intensity
  77: "❄️", # Snow grains
  80: "🌧️", # Rain showers: Slight intensity
  81: "🌧️", # Rain showers: Moderate intensity
  82: "🌧️", # Rain showers: Violent intensity
  85: "❄️", # Snow showers: Slight intensity
  86: "❄️", # Snow showers: Heavy intensity
  95: "⛈️", # Thunderstorm: Slight or moderate
  96: "⛈️", # Thunderstorm with slight hail
  99: "⛈️", # Thunderstorm with heavy hail
}

wmo_texts = {
      0: "Clear sky",
      1: "Mainly clear",
      2: "Partly cloudy",
      3: "Overcast",
      45: "Fog",
      48: "Depositing rime fog",
      51: "Light drizzle",
      53: "Moderate drizzle",
      55: "Dense drizzle",
      56: "Light freezing drizzle",
      57: "Dense freezing drizzle",
      61: "Slight rain",
      63: "Moderate rain",
      65: "Heavy rain",
      66: "Light freezing rain",
      67: "Heavy freezing rain",
      71: "Slight snow fall",
      73: "Moderate snow fall",
      75: "Heavy snow fall",
      77: "Snow grains",
      80: "Slight rain showers",
      81: "Moderate rain showers",
      82: "Violent rain showers",
      85: "Slight snow showers",
      86: "Heavy snow showers",
      95: "Slight or moderate thunderstorm",
      96: "Thunderstorm with slight hail",
      99: "Thunderstorm with heavy hail",
}

async def on_fetch(request, env):
    cached = await env.KV.get("weather")
    if cached:
        cached = json.loads(cached)

    if not cached:
        cached = {'latitude': 52.23009, 'longitude': 21.017075, 'generationtime_ms': 0.286102294921875, 'utc_offset_seconds': 7200, 'timezone': 'Europe/Berlin', 'timezone_abbreviation': 'CEST', 'elevation': 113.0, 'hourly_units': {'time': 'iso8601', 'temperature_2m': '°C', 'relative_humidity_2m': '%', 'wind_speed_10m': 'km/h', 'apparent_temperature': '°C', 'precipitation_probability': '%', 'precipitation': 'mm', 'weather_code': 'wmo code'}, 'hourly': {'time': ['2024-10-26T18:00', '2024-10-27T00:00', '2024-10-27T06:00', '2024-10-27T12:00'], 'temperature_2m': [12.9, 8.3, 9.3, 14.3], 'relative_humidity_2m': [78, 88, 86, 67], 'wind_speed_10m': [6.1, 7.6, 5.0, 7.2], 'apparent_temperature': [11.8, 6.3, 7.8, 12.8], 'precipitation_probability': [2, 0, 0, 0], 'precipitation': [0.0, 0.0, 0.0, 0.0], 'weather_code': [0, 0, 3, 0]}, 'daily_units': {'time': 'iso8601', 'sunrise': 'iso8601', 'sunset': 'iso8601'}, 'daily': {'time': ['2024-10-26'], 'sunrise': ['2024-10-26T07:21'], 'sunset': ['2024-10-26T17:18']}}

    if (time.time() - cached.get("ts", 0)) > 60:
        cached['stale'] = '%dh' % round((time.time() - cached.get("ts", 0)) / 60 / 60)
        u = "https://api.open-meteo.com/v1/forecast?latitude=52.2298&longitude=21.0118&hourly=temperature_2m,relative_humidity_2m,wind_speed_10m,apparent_temperature,precipitation_probability,precipitation,weather_code&forecast_days=1&temporal_resolution=hourly_6&forecast_hours=24&timezone=Europe%2FBerlin&daily=sunrise,sunset"
        a = await fetch(u)
        result = await a.text()
        print(result)
        result = json.loads(result)
        if 'hourly' in result:
            cached = result
            print(repr(result))
            cached['stale'] = False
            cached['ts'] = time.time()
            await env.KV.put("weather", json.dumps(cached))

    wt = cached
    if "hourly" not in wt:
        wt["hourly"] = {}
    wt["hourly"]["wmo_icon"] = []
    wt["hourly"]["wmo_text"] = []
    for wmo in wt["hourly"]["weather_code"]:
        wt["hourly"]["wmo_icon"].append( wmo_icons[wmo] )
        wt["hourly"]["wmo_text"].append( wmo_texts[wmo] )

    dusk = datetime.fromisoformat(wt["daily"]["sunrise"][0]).strftime("%H:%M")
    dawn = datetime.fromisoformat(wt["daily"]["sunset"][0]).strftime("%H:%M")
    t = pyratemp.Template(temp)
    d = datetime.today().strftime("%a %d %b %Y")
    return Response.new(t(w=wt, date=d,dusk=dusk, dawn=dawn, hour_steps=list(range(len(wt["hourly"]["time"])))), 
        headers=[('content-type', 'text/html')],
    )

