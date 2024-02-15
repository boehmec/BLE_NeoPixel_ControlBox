### This is a small project for underground LED for my electric mountainboard.

Use e.g. NRF connect on your phone to send the commands to the published BLE service.
Search for the device with name "BLE NeoPixel Board Control". Connect.

You can use it also for other stuff.

## VSCode vs Arduino

I startetd coding with Arduino IDE but found it to be very cumbersome the bigger the projects get. Thats why I switch more and more projects I make over to VSCode.

## 3D files to print

I designed a simple box to hold all electronics, like ESP32-C3, a LiIon Charger, the LiIon Holder with LiIon (18650) and a 5V step up converter for the NeoPixel LEDs.
![3D Model](/img/3d-model.png)


## Commands: 0-9

You can send 10 different commands to the control box, see below:

- 0: turn color to red
- 1: turn color to green
- 2: turn color to blue
- 3: start pulsing current color, and make pulsing also faster
- 4: start pulsing current color, and make pulsing also slower
- 5: current brightness darker
- 6: current brightness brighter
- 7: max brightness darker
- 8: max brightness brighter
- 9: reset to default


## Schematic

    +------------------------+
    |        Step-up         |
    |      Converter         |
    |                        |
    |   Vin ---- Power Switch| (Voltage from LiPo)
    |   Vout ---+------> LED Strip (5V)/ ESP32 5V
    |   GND ---- GND          |
    +------------------------+
           |
           |  +--------------+
           |  |   ESP32-C3   |
           |  +--------------+
           |      |     |
           |      |     +----> Pin 4 ----> LED Strip
           |      |
           |      +----> Pin 5 ----> Power Switch LED (Positive)
           |      |
           |      +----> Pin 6 ----> Power Switch LED GND (Negative)
           |      |
           |      +----> +5V   ----> Step-up Converter (+5V)
           |      |
           |      +----> GND   ----> Step-up Converter (GND)
           |
           +------------------------+
                   |
                   |
    +--------------|-------------+
    |  LiPo Charger Circuit      |
    |                            |
    |  +----> Battery (+)        |
    |  |                         |
    |  +----> Battery (-)        |
    |  |                         |
    |  +----> Vout ----> Power Switch (Normally Closed)
    |  |                         |
    |  +----> GND                 |
    +-----------------------------+

## BOM

| Component  | Link |
| ------------- | ------------- |
| ESP32-C3  | https://de.aliexpress.com/item/1005006170575141.html |
| 5V Mini-Step-Up-Boost | https://de.aliexpress.com/item/1005006144998899.html  |
| LiPo Charger Circuit | https://de.aliexpress.com/item/1005005865606098.html |
| LiIon Holder | https://de.aliexpress.com/item/32825161070.html |

