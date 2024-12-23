
This directory contains three pieces of software:

(1) Arduino code for ESP32 controller, that connects to WiFi, fetches
rendered binary blob from a worker and puts it on the e-paper display.

(2) Cloudflare worker (typescript) that renders a web page (with
puppeteer API), and rasterizes it to an Arduino-friendly binary
format.

(3) Cloudflare worker (python) that displays a simple HTML page with
current weather forecast.


Arduino ESP32 code
------------------

To draw on the e-paper dispaly, we need the EPD driver. The simplest
way to do that, is to use the headers from manufacturer examples. You
can download it from:

  https://www.waveshare.com/wiki/E-Paper_ESP32_Driver_Board#Download_Demo

To make it easy to use from another projects, you can put `epd.h` in
the path Arduino IDE can read, example on Linux:

    mkdir ~/Arduino/libraries/epd/
    cp ~/Downloads/E-Paper_ESP32_Driver_Board_Code/Loader_esp32wf/*h ~/Arduino/libraries/epd/

With that in place, a typical C include from an Arduino project should
now work:

    #include <epd.h>

Assuming the EPD headers work, you can open the Arduino project from

    ESP32-fetch-from-worker/ESP32-fetch-from-worker.ino

You need to fill a couple of constants:

  - WiFi SSID
  - WiFi password
  - url to fetch the data from (remember about the `&binary=1` parameter)
  - EPD display variant

Remember to set the Arduino Board type as "ESP32 Dev Module" as per instructions

    https://www.waveshare.com/wiki/Arduino_ESP32/8266_Online_Installation

Cloudflare worker - render-raster
---------------------------------

First, you need npm:

    sudo apt install npm

Install the dependencies in the worker-render-raster directory:

    cd worker-render-raster
    npm install @cloudflare/puppeteer --save-dev
    npm install fast-png --save-dev

You also need to create a KV namespace:

    npx wrangler kv:namespace create KV
    npx wrangler kv:namespace create KV --preview

This will ask you for Cloudflare account access, and return KV id
values. Put them in `wrangler.toml`.  Now you should be ready run the
code, you can start without deploying first:

    npx wrangler dev --remote

Finally you can deploy it to Cloudflare with simple:

    npx wrangler deploy


Clodudflare worker - weather panel
----------------------------------

Go to the worker-weather-panel directory, create the KV:

    cd worker-weather-panel
    npx wrangler kv:namespace create KV
    npx wrangler kv:namespace create KV --preview

Fill the `id` fields in `wrangler.toml` and similarly, work or deploy
the worker:

    npx wrangler dev --remote
    npx wrangler deploy
