# Vertiglow
A Northwestern University [AutoAquaponics](https://autoaquaponics.org/) project to help plants grow!

## About
The goal of this project is to control a linear actuator that moves lights vertically towards or away from growbeds so that the light source is the optimal distance from the plants. At different ages, plants have different distances they should be from light sources for optimal growth. To make this happen, we need to take pictures of the plants, use computer vision algorithms to determine the height of the plants relative to the light source, determine how much that distance needs to change, and commute this information to the linear actuators. Vertiglow started as a DTC project. See the AutoAquaponics [Google drive folder](https://drive.google.com/drive/folders/1XSqfYOb2gLW_ThH4O2OlnaK0mj552j5d) for more details.

## Technologies
### Hardware
We are using the ESP32-cam cam board with a OV2640 camera module. To program the device, we are using a ESP32-CAM-MB micro USB programmer. Various arduino examples and frameworks may refer to this board as the AI THINKER model.

### Software
The ESP32 is very memory-constrained, so we are using C++ rather than python. For image processing, we're using the [opencv](https://opencv.org/) library. However, ESP32 does not have enough computing resources to run the full opencv library. We are using a [modified version](https://github.com/joachimBurket/esp32-opencv) of the library designed to be cross-compiled on ESP32. Pre-compiled binaries for the library are already located in the main/opencv folder in this repo. We are also using the [esp32-camera library](https://github.com/espressif/esp32-camera) from Espressif.

### Framework
There are many frameworks for working with ESP32 such as Arduino IDE or Platform IO. For Vertiglow, we are using the Espressif IoT Development Framework (ESP-IDF) because it easily allows us to link pre-compiled binaries for external libraries such as opencv.

## Getting Started
First, install ESP-IDF. Espressif recommends using the VS Code ESP-IDF extension, but for this project we recommend [manually installing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#manual-installation) and using via the command line since that is what we've had success getting to work. 

Next, clone this repo anywhere on your computer. Start the esp-idf command line, which will automatically set up all the necessary environment variables. On Windows, search for the ESP-IDF cmd or ESP-IDF powershell executables (it's your choice which to use). All ESW-NU members on this project so far been Windows users, so we are are not sure exactly what it is on macOS, but you should be able to find it by searching ESP-IDF. Navigate to the cloned repo from the esp-idf command line. Be sure to run the following commands from the root of the project folder (not /main or /build). First, set the target device, in our case the esp32.
```
idf.py set-target esp32
```
If any settings need to be changed (initially they shouldn't), you can run
```
idf.py menuconfig
```
Next, build the project. Especially for the first time, this may take awhile. There will be some C++ specific warnings such as deprecated enum methods or missing field initializers. These can be ignored.
```
idf.py build
```
Next, plug your esp32 into your computer's and [check what USB port](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html#check-port-on-windows) it is connected to. If the device doesn't show up, make sure your cable supports data transfer, not just charging. Note that if you don't specify a port, then esp-idf will try all ports until it finds one with the appropriate device connected. 
```
idf.py -p <port> flash
```
After the program is flashed to the devices, to see log and print statements and check the program's status, we can use the serial monitor. The (print-filter)[https://docs.espressif.com/projects/esp-idf/en/v4.4.3/esp32/api-guides/tools/idf-monitor.html#output-filtering] argument specifies the maximum level of log statements you wish to see. Be sure to use it so that the monitor is not flooded with verbose log statements.
```
idf.py -p <port> monitor --print-filter="*:I"
```

### Viewing Image Data
Instead of setting up web streaming, we are using esp-idf's logging capabilities to transmit raw image data to our local machine over the serial wired connection. We are using log level verbose and the tag "DATA_TAG" to indicate that a log statement corresponds to image data. The python script viewer.py parses these logs and displays images. To get started, install the libraries required by the python script.
```
pip install -r requirements.txt
```
Next, after flashing the program to the esp32, run the following command to start the serial monitor, filter by verbose statements with DATA_TAG, and save the output to a file.
```
idf.py -p COM10 monitor --print-filter="image_data:V" > log.txt
```
Point the esp-32 camera at test images or objects, and wait a bit since writing images to a file byte by byte using log statements is slow. Next, stop the monitor and run the python script. Images will pop up one at a time, and you must close the image window to view the next one.
```
python viewer.py log.txt
```
If this method seems a bit hacky, that's because it is. The esp-idf framework for starting a webserver seemed more complicated than Arduino's, and this was faster to get up and running. Using python's pyserial library to directly read from the serial monitor (instead of using the file as an intermediary) would be faster, but we were unable to get pyserial to register any data.

## Acknowledgements
- [esp-32 opencv library](https://github.com/joachimBurket/esp32-opencv)
- [ESW-NU](https://esw-nu.github.io/)
- Northwestern University

## Other Possible Vertiglow Solutions
### 1. Raspberry Pi
   
   This would make our lives easier (we could use python), but we couldn't get any of the raspberry pi cameras to work. It is also a larger piece of hardware.

### 2. ESP32-cam object classifier
   
   We train an object classification model on a normal computer, create an arduino library, run inference on the ESP-32, and use the top edge of the bounding box(es) on the plants. This is implemented with [Edge Impulse](https://edgeimpulse.com/) following followed this [tutorial](https://dronebotworkshop.com/esp32-object-detect/). AutoAquaponics has an account on Edge Impulse, if it's needed again please contact [Talia Ben-Naim](TaliaBen-Naim2025@u.northwestern.edu) for login details. So far we have a model trained on pictures taken from a laptop webcam. The model is not very accurate and we also don't have a good way of visualizing its performance for bounding box. Another problem with this method is that for best accuracy we may have to retrain the model for every new type of plant. 
### 3. Your idea here...
