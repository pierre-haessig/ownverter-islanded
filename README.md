# OwnVerter DCDC Application

This repository host embedded microcontroller code for using the [OwnTech OwnVerter](https://www.owntech.io/ownverter/) board as a DC/DC converter.

This code derives from the [OwnTech Power API Core repository](https://github.com/owntech-foundation/Core). It is designed to be used with VS Code and PlatformIO. The usage of this type of repository is documented at https://docs.owntech.org (e.g. Getting Started section).

## Usage

In the following, we assume that the OwnTech Getting Started guide has been sucessfully completed, which include all necessary software setup (VSCode with PlatformIO, Git...) on the host compture.

An [OwnTech OwnVerter](https://www.owntech.io/ownverter/) board is assumed to be connected to the host computer with a USB cable.

On this host computer:

1. **Clone this repository** to an empty folder (without space in the entire path to this folder)
2. **Open the folder in VSCode/PlatformIO**
   -  Remark: cloning can be done in PlatformIO's “Quick Access” view, select the “Miscelleanous / Clone Git Project” action
3. **Build and Upload** code to board's microcontroller
4. Open **Serial Monitor** to interact with the application

## Electric circuit schematic

to be detailed.


## Downloading OwnTech Power API Core

You fisrt need to download the Power API Core repository using the following command:

`git clone https://github.com/owntech-foundation/Core.git owntech_power_api`

Then, open VS Code and, if not already done, install the PlatformIO plugin.

Finally, open the newly cloned folder `owntech_power_api` using menu `File > Open Folder...`


## Project folder structure

While the project contains many folders and files, all the user developped code goes to the `src` folder. In the this folder, the file `main.cpp` is the entry point of the application.

The root project configuration is described in the `platformio.ini` file.

Other folders and files are used to configure the underlying Zephyr OS and PlatformIO, and are hidden by default.
