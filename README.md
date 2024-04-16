# This is a HexaDream.

It is an **6-voice** drone synthesizer with **low pass filter** and selectable waveforms for each voice. Device also has external footswitch control for changing root note.
It's power sides is appealing UI which uses 8x8 fully customizeable RGB LEDs, low cost and decent sound quality for the given purpose.

[pic](./pic.jpg)

## Software side

C++ fully open-source firmware, written with Arduino libraries. Sound side is covered with amazing and versatile [Mozzi library](), which is hard to wrap your head around first, but is very useable and capable, given hardware limitations.

LEDs management is taken care of by [FastLED library](https://fastled.io/), which makes animations and colors fluent and easy-to-use. firmware is placed here.

## Hardware side

Brain of the unit is ESP32 based development board, usual part name is ESP-WROOM-32. It drives regular 8x8 LED panel, made of WS2812B addressable LEDs. There is no separate DAC inside; audio is connected straight to ESP32 DAC out with several filters. Last schematic from [here](https://sensorium.github.io/Mozzi/learn/output/) is used, which makes perfect result for this hardware combination. Encoder is regular Aliexpress encoder with 5 contacts: 2 for button contact and 3 for encoder functionality. Schematic of hardware side is [here]().

## Enclosure

Handmade build box from pinewood with redwood veneer glued on all sides; top cover is made with 1 mm aluminium plate. Everything is covered with 2 layers of acrylic laqueir.

## How to Use:
from right to left, you can see on the screen:
- 6 vertical columns, representing level of separate oscnoise
- each column's color represents selected wave for this oscillator
- 2 left-most columns representing LPF, left represents resonance and right represents cutoff frequency
- by default, all levels are randomized slightly from time to time, adding a bit of movement
- encoder has different functions depending on current mode. It can be used to change values on specific column, or change columns. Mode is changed by button press.
- On load, mode is: ```0 5 7 12 24 36 ```. Button press on leftmost column rotate voices formula through the following values:
  -  ```0 5 7 12 24 36 ``` - **sus4**
  -  ```0 4 7 12 24 36 ``` - **maj**
  -  ```0 2 7 12 24 36 ``` - **sus2**
  -  ```0 3 7 12 24 36 ``` - **min**




## TODO for future releases:

- overall volume control
- more musical gain reduce/increase
- more visually appealing effects for better UI
- vibrato on oscs
- options for controlled movement of voices
- more waveforms
- work on LPF (more models/effects instead of LPF)
