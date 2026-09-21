<h1 align="center">Literary Quote Clock Rewrite in C++</h1>

This is a C++ rewrite of my [Literary Quote Clock](https://github.com/alextaschuk/Literary-Quote-Clock), which was originally written in Python.

It currently works for Waveshare's [6-inch IT8951 EPD](https://www.waveshare.com/6inch-hd-e-paper-hat.htm). The program has all of the clock's original functionality and includes some improvements as well.

<p align="center">
    <img src="share/examples/demo.png" alt="A quote from \"Dune\" by Frank Herbert for 12:00 that reads \"The man crawled across a dune top. He was a mote caught in the glare of the noon sun\"" height="400"/>
</p>

## Table of Contents
<details>
<summary>Click to View</summary>

1. [How to Set up the Clock](#how-to-set-up-the-clock)
2. [Text Formatting](#text-formatting)
3. [Functionality Improvements](#functionality-improvements)

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

3. `cd` into the Repository and make a build folder:

    ```sh
    mkdir build && cd build && cmake .. && cd ..
    ```

4. Modify the `WorkingDirectory` in [CPP_clock_build.service](scripts/CPP_clock_build.service) to point to the build folder you just made.

    - This script is ran once during the Pi's startup to compile the program.

5. Modify the `ExecStart` in [CPP_clock.service](scripts/CPP_clock.service) to point to the `clock` binary in the build/ folder.

    - This script starts the clock program after CPP_clock_build.service has run.

6. Move the scripts to /etc/systemd/system with:

    ```sh
    mv scripts/CPP_clock_build.service /etc/systemd/system/CPP_clock_build.service

    mv scripts/CPP_clock.service /etc/systemd/system/CPP_clock.service
    ```

7. Reload the systemd manager so that it sees the new service file:

    ```sh
    sudo systemctl daemon-reload
    ```

8. Enable the scripts and start the clock:

    ```sh
    sudo systemctl enable --now CPP_clock_build.service
    sudo systemctl enable --now CPP_clock.service
    ```


## Text Formatting

In some instances, it may be worth preserving some or all of a quote's original formatting. One or more characters in a string of text can be wrapped with a delimiter to have the formatting be applied to the text.

### Italic `_` (U+005F, Low Line/Underscore)

Wrap text with this character to _italicize_ it. This may be combined with the bold delimiter to make the text ***italic and bold***.

For example, the CSV stores:

> Henry held out his hand for the note, which Victoria gave over in exchange for a Sweet Caporal. There were only four words: _Tomorrow morning. 2 o’clock_.

Which will be formatted as:

<p align="center">
    <img src="share/examples/italic-formatting.png" height="400"/>
</p>

### Bold `*` (U+002A, Asterisk)

Wrap text with this character to *bold* it. This may be combined with the bold delimiter to make the text ***italic and bold***.

There aren't any quotes yet where the bold delimiter has been needed, but I have added it as an option for future quotes. It is also used when an error message is printed to the screen.


### Newline / End of line `\n` (U+000A, End of Line)

Any succeeding characters in a word after this character are put on a new line.

For example, the CSV stores:

> He smiled to himself and went to his office and waited for the telephone call that he knew would come. \nIt came at two o’clock that afternoon.

<p align="center">
    <img src="share/examples/newline-comparison.png" height="400"/>
</p>

This delimiter can be used in succession of itself to add white space between lines of text.

For example, the CSV stores:

> A full one hundred meters down the slope, Kazuo Kiriyama didn't look back. Instead, he glanced down at his watch. \n\nThe second hand had just made its seventh click past five.

<p align="center">
    <img src="share/examples/double-newline-comparison.png" height="400"/>
</p>

### Escape Character `\` (U+005C, Reverse Solidus/Backslash)

Add this character before a formatting delimiter to have the string literal version of the delimiter printed on the image.

For example, the CSV stores:

> “I will be a sonofa\*\*\*\*h if he ain’t in here at eleven-thirty at night, fartin’ around in the dark with a pair of scissors and a paper sack.”

Which will be formatted as:

<p align="center">
    <img src="share/examples/escape-formatting.png" height="400"/>
</p>


## Functionality Improvements

Writing this project in C++ gave me a lot more freedom in converting quotes from a CSV to images.

### Better optimized text wrapping


### Improved horizontal and vertical glyph spacing.


### More accurate custom text formatting.


### Benchmark for Saving Images


### Benchmark for Displaying Images on Clock
