matelight controller
====================

Games for matelight WLED display with doorbell support.

This will run games on a 10x20 Matelight display. This program will
communicate with the Matelight WLED controller with UDP on port 21324.
It can either communicate with the WLED controller on a static IP or
find the IP of the WLED controller with MDNS. It will use a USB joystick
for input.

Games implemented:
------------------
- Snake
- Tetris (Original implementation: https://github.com/mamikk/tetris )

Input:
------
- Start: Start/Pause game
- Select: Switch game
- Up/Down/Left/Right/A/B: Game specific

Build:
------
Requirements: libavahi-client, libcurl, libmosquitto.
```
make
./matelight mdns:Matelight 21324 /dev/input/js0
```

TODO:
-----
- MQTT/JSON API for text
- More games: https://gamedev.stackexchange.com/questions/8155/styles-of-games-that-work-at-low-resolution/175311#175311
