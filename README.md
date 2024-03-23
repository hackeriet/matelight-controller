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
- Snake (Original implementation: https://github.com/mamikk/snake )
- Tetris (Original implementation: https://github.com/mamikk/tetris )
- Flappy bird
- Pong
- Breakout

Input:
------
- Start: Start/Pause game
- Select: Switch game
- Up/Down/Left/Right/A/B: Game specific

Build:
------
Requirements: libavahi-client, libcurl, libudev, libmosquitto.
```
make
```

Run:
----
```
./matelight --mdns-description=Matelight --port=21324 --joystick-device=/dev/input/js0
```

Run locally with simulator:
---------------------------
```
mkfifo /tmp/js0.fifo
./contrib/matelight-simulator.py --address=127.0.0.1 --port=21324 --fifo=/tmp/js0.fifo &
./matelight --address=127.0.0.1 --port=21324 --joystick-device=/tmp/js0.fifo
```

TODO:
-----
- Games:
  - Tetris: Increase speed
  - Pong/Breakout: Fix floating point rounding bugs
  - Pong: AI
  - Space Invaders
  - More games: https://gamedev.stackexchange.com/questions/8155/styles-of-games-that-work-at-low-resolution/175311#175311
- MQTT:
  - Hackerspace Open/Closed
  - MQTT/JSON API for text
- UI:
  - Game over screen
