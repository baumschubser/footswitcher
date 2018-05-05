
# footswitcher
With footswitcher you use a [cheap USB pedal](http://www.pcsensor.com/one-switch-pedal.html) to switch easily between "insert mode" and "escape mode" in [Vim](https://www.vim.org/). In fact, you can switch between any kind of keys to be pressed.

There exist already [hardware hacks](https://github.com/alevchuk/vim-clutch) for using this kind of USB pedal to switch Vim modes. footswitcher works without any hardware modification and also with more than two words to switch between.

## Configuration
To configure the series of key presses that you want to use, please use [footswitch](https://github.com/rgerganov/footswitch). You define a series of key presses (by its [hex values](http://www.freebsddiary.org/APC/usb_hid_usages.php)), separated by "FF".

### For switching vim modes
    footswitch -S '29 0C FF 29'
Here the first press triggers the keys Escape and "i" (hex values 29 and 0c). The second pedal press triggers Escape. The third press would trigger Escape and "i" again.

### Hello world
    footswitch -S '0B 08 0F 0F 12 FF 2C FF 1A 12 15 0F 07'
Here the first press triggers "hello". The second pedal press triggers " " (space) and the third press triggers "world".  The fourth press would trigger "hello" again.

## Running
To use footswitcher, just run

    footswitcher
The software should automatically find the correct pedal device and send the virtual key presses in the name of the keyboard device.
If this fails, please specify the correct device files like so:

     footswitcher -i /dev/input/event4 -o /dev/input/event1

With `event4` being the USB pedal device file and `event1` being the keyboard device file (use `lsusb` to find the correct device files on your system).

## Credits
footswitcher stands on the shoulders of [footswitch](https://github.com/rgerganov/footswitch) by Radoslav Gerganov.

## License
Copyright 2018 by Matthias Clausen <matthiasclausen@posteo.de>.

MIT License (see LICENSE file).
