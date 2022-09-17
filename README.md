# OGO Dry Seperating Toilet Upgrade

This projects aims to improve the OGO Toilet by using a much better 120x120x25mm ultra silient fan to replace the original 40x40x10mm.
It ensures better drying within the container and thus significantly longer usability.
It also actively prevents odors that can otherwise arise.

## Why?

We are traveling with 5 people, sometimes with 6 people in the camper.
The amount of toilet trips that result overloads the drying capacity of the toilet, which is probably designed to be used only occasionally.

## Bill of Materials (BOM)

You need a 3D printer or 3D printing service for the Ventilation suction funnel.

* 3D printed inlet [OnShape](https://cad.onshape.com/documents/5fab3112d90d7793569932e3/w/34eccf5e19cc8eaf718aa94e/e/3b89bd3ae0b6f4b1214b51a0?renderMode=0&uiState=632604f88155894514c5c737) 
* 3D printed spacer [OnShape](https://cad.onshape.com/documents/0ea4d1af98224392b9e8ed38/w/a983284bd5da82020f432b6a/e/7f57e93fc929c16fac4443e2?renderMode=0&uiState=63260539acff487b28202a53)
* 3D printed outlet [OnShape](https://cad.onshape.com/documents/5fab3112d90d7793569932e3/w/34eccf5e19cc8eaf718aa94e/e/3b89bd3ae0b6f4b1214b51a0?renderMode=0&uiState=632604f88155894514c5c737)
* 120x120x25mm 12V PWM 4pin fan [Amazon](https://www.amazon.de/gp/product/B07GSRRHZT)
* Arduino Nano [Amazon](https://www.amazon.de/gp/product/B01MS7DUEM)
* 2-pin, 3-pin, 4-pin JST-XH Male Plug and Female housing Socket
* 10kΩ potentiometer
* 1MΩ resistor
* 250kΩ resistor
* Empty PCB
* Housing for electronics [OnShape](https://cad.onshape.com/documents/cc59e88626d9c5d25e4a785c/w/d29ec8a2df2736b70b153abb/e/2e341e9a070032564e845e18?renderMode=0&uiState=632606f310f3923766c22f6e)

## Pictures of the built module
<img src="images/fan1.jpg?raw=true" alt="Fan" width="50%"><br>
<img src="images/fan2.jpg?raw=true" alt="Fan outlet" width="25%">
<img src="images/fan3.jpg?raw=true" alt="Fan inlet" width="25%">

## Schematics
<img src="images/schematic.png?raw=true" alt="Schematic" width="50%">

## How to build this PlatformIO based project

1. [Install PlatformIO Core](http://docs.platformio.org/page/core.html)
2. Run these commands:

```
    # Change directory into the code folder
    > cd womolin.ogo-ttt-fan-upgrade

    # Build project
    > platformio run -e nanoatmega32 

    # Upload firmware
    > platformio run -e nanoatmega32 --target upload
```

## License

Fully (c) by Martin Verges.

This Project is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work.
If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
