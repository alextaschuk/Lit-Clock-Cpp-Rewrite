<h1 align="center">Literary Quote Clock Rewrite in C++</h1>

This is a C++ rewrite of my [Literary Quote Clock](https://github.com/alextaschuk/Literary-Quote-Clock), which was originally written in Python.

It currently works for Waveshare's [6-inch IT8951 EPD](https://www.waveshare.com/6inch-hd-e-paper-hat.htm). The program has all of the clock's original functionality and includes some improvements as well.

## Table of Contents
<details>
<summary>Click to View</summary>

1. [How to Set up the Clock](#how-to-set-up-the-clock)
2. [Functionality Improvements](#functionality-improvements)

</details>

## How to Set up the Clock

### Materials

- [Waveshare 6-inch E-ink Display & HAT](https://www.waveshare.com/6inch-hd-e-paper-hat.htm)
- [Raspberry Pi](https://www.raspberrypi.com/products/)

### Setting up the Clock

1. Complete steps 1 and 2 on the [Working with Raspberry Pi (SPI)](https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#Working_with_Raspberry_Pi_.28SPI.29) section on Waveshare's wiki page.

2. Clone this repository recursively onto the Pi (necessary to enable logger functions via spdlog) with:

    ```sh
    git clone --recursive https://github.com/alextaschuk/Lit-Clock-Cpp-Rewrite.git
    ```

    - *Note*: If you forgot the `--recursive` flag, run `git submodule update --init` to clone the logging library locally.

3. `cd` into the Repository and run:

    ```sh
    mkdir build && cd build
    ```

4. Compile the program:

    ```sh
    cmake .. && cmake --build .
    ```

5. Modify the `ExecStart` in [CPP_clock.service](CPP_clock.service) to store the path to the `clock` binary in the build/ folder.

6. Move CPP_clock.service to /etc/systemd/system with:

    ```sh
    mv CPP_clock.service /etc/systemd/system/CPP_clock.service
    ```

7. Reload the systemd manager so that it sees the new service file:

    ```sh
    sudo systemctl daemon-reload
    ```

8. Start the clock:

    ```sh
    sudo systemctl enable --now CPP_clock.service
    ```

## Functionality Improvements

Writing this project in C++ gave me a lot more freedom in converting quotes from a CSV to images.

### Better optimized text wrapping


### Improved horizontal and vertical glyph spacing.


### More accurate custom text formatting.


### Benchmark for Saving Images


### Benchmark for Displaying Images on Clock
