/*
 Copyright 2014-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//
// blinkenLightsPi: Run a blinkenLights display on a Raspberry Pi framebuffer.
//
// This code is targeted for a AdaFruit 2441 3.5" PiTFT+ 480x320 display
//

// Setup
//
//  as root:
// 
// apt-get update -y
// apt-get upgrade -y
// rpi-update
// reboot
// wget https://raw.githubusercontent.com/notro/rpi-source/master/rpi-source -O /usr/bin/rpi-source && sudo chmod +x /usr/bin/rpi-source && /usr/bin/rpi-source -q --tag-update
// apt-get install bc
// rpi-source
// apt-get install libncurses5-dev
// 
// cd /root/linux
// vi drivers/staging/fbtft/fb_ili9340.c
// ; change to
// ;  #define WIDTH           320
// ;  #define HEIGHT          480
// make prepare
// make SUBDIRS=drivers/staging/fbtft/ modules
// make SUBDIRS=drivers/staging/fbtft/ modules_install
// depmod
// 
// /etc/modules-load.d/fbtft.conf:
// 
//     fbtft_device
//     
// 
// /etc/modprobe.d/fbtft.conf
// 
//     options fbtft_device name=pitft txbuflen=32768
//
// The following step my be needed before the modprobe: Run "raspi-config"; select "5 Interfacing options", "P4 SPI", "Yes" in response to "Would you like the SPI interface to be enabled? "
// 
// reboot.
// 
// Not loaded on reboot....
// 
// sudo modprobe fbtft_device
// 
// [   43.109336] fbtft_device: module is from the staging directory, the quality is unknown, you have been warned.
// [   43.110709] spidev spi0.0: spidev spi0.0 500kHz 8 bits mode=0x00
// [   43.110731] spidev spi0.1: spidev spi0.1 500kHz 8 bits mode=0x00
// [   43.110772] bcm2708_fb soc:fb: soc:fb id=-1 pdata? no
// [   43.110824] spidev spi0.0: Deleting spi0.0
// [   43.111783] fbtft_device: GPIOS used by 'pitft':
// [   43.111810] fbtft_device: 'dc' = GPIO25
// [   43.111844] spidev spi0.1: spidev spi0.1 500kHz 8 bits mode=0x00
// [   43.111866] spi spi0.0: fb_ili9340 spi0.0 32000kHz 8 bits mode=0x00
// [   43.141003] fb_ili9340: module is from the staging directory, the quality is unknown, you have been warned.
// [   43.358297] graphics fb1: fb_ili9340 frame buffer, 320x480, 300 KiB video memory, 4 KiB DMA buffer memory, fps=20, spi0.0 at 32 MHz
// 

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

/*
 * fbtestfnt.h
 *
 * 
 *
 * Original work by J-P Rosti (a.k.a -rst- and 'Raspberry Compote')
 *
 * Licensed under the Creative Commons Attribution 3.0 Unported License
 * (http://creativecommons.org/licenses/by/3.0/deed.en_US)
 *
 * Distributed in the hope that this will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

// Add'l font from http://www.piclist.com/techref/datafile/charset/8x8.htm

#define FONTW 8
#define FONTH 8

char fontImg[][FONTW * FONTH] =
  {
    { // ' ' (space)
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // !
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // "
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // # 
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // $ (TODO)
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // % (TODO)
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // & (TODO)
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 1, 0, 1, 0, 1, 0, 0,
            0, 0, 1, 0, 1, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0
    },
    { // '
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // (
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // )
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // * (TODO)
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // +
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // ,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0
    },
    { // -
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // .
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // /
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    {  // 0 (zero)
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 1, 1, 0,
            0, 1, 0, 0, 1, 0, 1, 0,
            0, 1, 0, 1, 0, 0, 1, 0,
            0, 1, 0, 1, 0, 0, 1, 0,
            0, 1, 1, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // 1
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 1, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },

    { // 2
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 1, 1, 1, 0, 0,
            0, 1, 1, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // 3 
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // 4 
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 1, 0, 0,
            0, 0, 0, 1, 0, 1, 0, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // 5 
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // 6 
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // 7
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // 8
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    {  // 9
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // :
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // ;
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0
    },
    { // <
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // =
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // >
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // ?
            0, 0, 1, 1, 1, 0, 0, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
// Use @ for lamp on

//    { // @
//            0, 0, 1, 1, 1, 1, 0, 0,
//            0, 1, 0, 0, 0, 0, 1, 0,
//            1, 0, 0, 1, 1, 0, 1, 0,
//            1, 0, 1, 0, 1, 0, 1, 0,
//            1, 0, 0, 1, 1, 1, 0, 0,
//            0, 1, 0, 0, 0, 0, 1, 0,
//            0, 0, 1, 1, 1, 1, 0, 0,
//            0, 0, 0, 0, 0, 0, 0, 0
//    },

    { // @
            0, 0, 1, 1, 1, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 0,
            1, 1, 1, 1, 1, 1, 1, 0,
            1, 1, 1, 1, 1, 1, 1, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 1, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },

    { // A
            0, 0, 0, 1, 1, 0, 0, 0,
            0, 0, 1, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // B
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // C
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // D
            0, 1, 1, 1, 1, 0, 0, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 1, 1, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // E
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // F
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // G
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // I
            0, 0, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // J
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // K
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 1, 0, 0, 0,
            0, 1, 0, 1, 0, 0, 0, 0,
            0, 1, 1, 1, 0, 0, 0, 0,
            0, 1, 0, 0, 1, 0, 0, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0
    },
    { // L
            1, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // M
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // N
            1, 0, 0, 0, 0, 0, 1, 0,
            1, 1, 0, 0, 0, 0, 1, 0,
            1, 0, 1, 0, 0, 0, 1, 0,
            1, 0, 0, 1, 0, 0, 1, 0,
            1, 0, 0, 0, 1, 0, 1, 0,
            1, 0, 0, 0, 0, 1, 1, 0,
            1, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // O
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // P
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // Q
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 1, 0, 1, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 0, 1, 1, 1, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0  
    },
    { // R
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 1, 0, 0, 0,
            0, 1, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // S
            0, 1, 1, 1, 1, 1, 0, 0,
            1, 0, 0, 0, 0, 0, 1, 0,
            1, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            1, 0, 0, 0, 0, 0, 1, 0,
            0, 1, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // T
            1, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // U
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 1, 0, 0, 0, 0, 1, 0,
            0, 0, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // V
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // W
            1, 0, 0, 0, 0, 0, 1, 0,
            1, 0, 0, 1, 0, 0, 1, 0,
            1, 0, 0, 1, 0, 0, 1, 0,
            1, 0, 0, 1, 0, 0, 1, 0,
            1, 0, 0, 1, 0, 0, 1, 0,
            1, 0, 0, 1, 0, 0, 1, 0,
            0, 1, 1, 0, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // Z
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    // [\]^` (TODO)
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    },
    { // _
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
    },
    { // ` (TODO)
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 1, 1, 1, 1
    }
    // a-z (TODO) ...
    // {|}~ (TODO) ...

  };

// get the 'image'/'pixel map' index
// for a given character - returns 0 (space) if not found

int font_index(char a)
  {
    int ret = a - 32;
    // is the value in the 'printable' range (a >= 32) and 
    // within the defined entries
    if ((ret >= 0) && ( ret < (int) (sizeof(fontImg) / (FONTW * FONTH) ) ) )
      {
        ret = a - 32; // we start at zero
      }
    else
      {  // if not, return 0 (== space character)
        ret = 0;
      }
    return ret;
}

typedef unsigned short int pxl;

// 'global' variables to store screen info
static pxl *fbp = 0;
static unsigned int bytes_per_pixel = 2;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
static pxl bg_col = 0;
static pxl fg_col = 0xffff;

// 'plot' a pixel in given color

void put_pixel (unsigned int x, unsigned int y, pxl c)
  {
    // calculate the pixel's byte offset inside the buffer
    unsigned int pix_offset = (unsigned int) (x + y * finfo.line_length/bytes_per_pixel);
    *(fbp + pix_offset) = c;
  }

void fill_rect (unsigned int x, unsigned int y, unsigned int w, unsigned int h, pxl c)
  {
    unsigned int cx, cy;
    for (cy = 0; cy < h; cy ++)
      {
        for (cx = 0; cx < w; cx ++)
          {
            put_pixel (x + cx, y + cy, c);
          }
      }
  }

static pxl grey = 0x8410;

// Draw a text string on the display

static void draw (unsigned int textX, unsigned int textY, char *arg)
  {

    // Convert from text row/col to pixel coordinates.
    textX *= FONTW;
    textY *= FONTH;

    char * text = arg;

    unsigned int i, l, x, y;

    // loop through all characters in the text string
    l = (unsigned int) strlen (text);
    for (i = 0; i < l; i ++)
      {
        char ch = text [i];
        if (ch == 'n') // newline
          {
            textX = 0;
            textY += FONTH;
            continue;
          }
        pxl color = fg_col;
        // Draw '+' as a white circle
        if (ch == '+')
          {
            ch = '@';
          }
        // Draw '-' as a black circle
        else if (ch == '-')
          {
            ch = '@';
            color = bg_col;
          }
        // get the 'image' index for this character
        int ix = font_index (ch);
        // get the font 'image'
        char * img = fontImg [ix];
       // loop through pixel rows
        for (y = 0; y < FONTH; y ++)
          {
            // loop through pixel columns
            for (x = 0; x < FONTW; x ++)
              {
                // get the pixel value
                char b = img [y * FONTW + x];
                if (b > 0)
                  { // plot the pixel
                    put_pixel (textX + x, textY + y, color);
                  }
                else
                  {
                    put_pixel (textX + x, textY + y, grey);
                  }
              } // end "for x"
          } // end "for y"
        textX += FONTW;
      } // end "for i"
  }


#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_state.h"
#include "shm.h"

struct system_state_s * system_state;
vol word36 * M = NULL;                                          // memory
vol cpu_state_t * cpus;
vol cpu_state_t * cpun;


// Draw a number on the screen in binary. draw() will draw '+' as a white
// circle and '-' as a black circle.
// 'n' is the number of bits in the number, 'v' is the value to draw.


static char buf [128];
static void draw_n (int n, word36 v, unsigned int col, unsigned int row)
  {
    char * p = buf;
    for (int i = n - 1; i >= 0; i --)
      {
        * p ++ = ((1llu << i) & v) ? '+' : '-';
      }
    * p ++ = 0;
    draw (col, row, buf);
  }

//
// Usage: blinkenLightsPi [cpu_number]
//

int main (int argc, char * argv [])
  {

// Attach the DPS8M emulator shared memory

// Get the optional CPU number from the command line

    int cpunum = 0;
    if (argc > 1 && strlen (argv [1]))
      {
        char * end;
        long p = strtol (argv [1], & end, 0);
        if (* end == 0)
          {
            cpunum = (int) p;
            argv [1] [0] = 0;
          }
      }
    if (cpunum < 0 || cpunum > N_CPU_UNITS_MAX - 1)
      {
        printf ("invalid cpu number %d\n", cpunum);
        return 1;
      }

// Open the emulator CPU state memory segment

    for (;;)
      {
        system_state = (struct system_state_s *)
          open_shm ("state", sizeof (struct system_state_s));

        if (system_state)
          break;

        printf ("No state file found; retry in 1 second\n");
        sleep (1);
      }

    M = system_state->M;
    cpus = system_state->cpus;

// Point to the selected CPU number

    cpun = cpus + cpunum;


// Open the framebuffer

    int fbfd = 0;
    size_t screensize = 0;

    fbfd = open("/dev/fb1", O_RDWR);
    if (! fbfd)
      {
        printf ("Error: cannot open framebuffer device.\n");
        return 1;
      }
    //printf ("The framebuffer device was opened successfully.\n");

    // Get variable screen information
    if (ioctl (fbfd, FBIOGET_VSCREENINFO, & vinfo))
      {
        printf ("Error reading variable information.\n");
        return 1;
      }
    //printf ("Original %dx%d, %dbpp\n", vinfo.xres, vinfo.yres,
            //vinfo.bits_per_pixel);
    bytes_per_pixel = vinfo.bits_per_pixel / 8;

    // Get fixed screen information
    if (ioctl (fbfd, FBIOGET_FSCREENINFO, & finfo))
      {
        printf ("Error reading fixed information.\n");
      }
    //printf("Fixed info: smem_len %d, line_length %d\n", finfo.smem_len, finfo.line_length);

// map fb to user mem

    screensize = finfo.smem_len;
    fbp = (pxl *) mmap (0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED,
                       fbfd, 0);

    if ((int) fbp == -1)
      {
        printf ("Failed to mmap.\n");
        return 1;
      }

// Set the screen

    // Paint the screen grey

    fill_rect (0, 0, vinfo.xres, vinfo.yres, grey);

    // Write the labels

//                 "0123456789012345678901234567890123456789"
    unsigned int l = 0;
    draw (0, l ++, "PRR ___ P _ PSR _______________");
    draw (0, l ++, "TRR ___ TSR _______________ TBR ______");
    draw (0, l ++, "IC  __________________");
    draw (0, l ++, "IWB ____________________________________");
    draw (0, l ++, "A   ____________________________________");
    draw (0, l ++, "Q   ____________________________________");
    draw (0, l ++, "E   ________ CA __________________");
    draw (0, l ++, "01 __________________ __________________");
    draw (0, l ++, "23 __________________ __________________");
    draw (0, l ++, "45 __________________ __________________");
    draw (0, l ++, "67 __________________ __________________");
    draw (0, l ++, "IR  __________________ RALR ___");
    draw (0, l ++, "TR  __________________________");
    draw (0, l ++, "0 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "1 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "2 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "3 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "4 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "5 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "6 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
    draw (0, l ++, "7 _______________ ___");
    draw (0, l ++, "  __ ____ __________________");
//DBSR  ADDR 24, BND 14 U 1 STACK 12
    draw (0, l ++, "ADDR ________________________");
    draw (0, l ++, "BND ______________ U _ STK ____________");
    draw (0, l ++, "FLT _____");
    draw (0, l ++, "SBF ____________________________________");
//                 "0123456789012345678901234567890123456789"


// Update values

    while (1)
      {
        l = 0;
        draw_n ( 3, cpun -> PPR.PRR,  4, l); 
        draw_n ( 1, cpun -> PPR.P,   10, l);
        draw_n (15, cpun -> PPR.PSR, 16, l);
        l ++;
        draw_n ( 3, cpun -> TPR.TRR,  4, l);
        draw_n (15, cpun -> TPR.TSR, 12, l);
        draw_n ( 6, cpun -> TPR.TBR, 32, l);
        l ++;
        draw_n (18, cpun -> PPR.IC,   4, l);
        l ++;
        draw_n (36, cpun -> cu.IWB,   4, l);
        l ++;
        draw_n (36, cpun -> rA,       4, l);
        l ++;
        draw_n (36, cpun -> rQ,       4, l);
        l ++;
        draw_n ( 8, cpun -> rE,       4, l);
        draw_n (18, cpun -> TPR.CA,  16, l);
        l ++;
        for (int i = 0; i < 8; i += 2)
          {
            draw_n (18, cpun -> rX [i + 0],  3,  l);
            draw_n (18, cpun -> rX [i + 1], 22,  l);
            l ++;
          }
        draw_n (18, cpun -> cu . IR,   4, l);
        draw_n ( 3, cpun -> rRALR,   28, l);
        l ++;
        draw_n (26, cpun -> rTR,     4, l);
        l ++;
        for (int i = 0; i < 8; i ++)
          {
            draw_n (15, cpun -> PAR [i] . SNR,     2, l); 
            draw_n ( 3, cpun -> PAR [i] . RNR,    18, l); 
            l ++;
            draw_n ( 2, cpun -> PAR [i] . AR_CHAR,    2, l);
            draw_n ( 4, cpun -> PAR [i] . AR_BITNO ,  5, l);
            draw_n (18, cpun -> PAR [i] . WORDNO, 10, l);
            l ++;
          }
        draw_n (24, cpun -> DSBR.ADDR, 5, l);
        l ++;
        draw_n (14, cpun -> DSBR.BND,    4, l);
        draw_n ( 1, cpun -> DSBR.U,     21, l);
        draw_n (12, cpun -> DSBR.STACK, 27, l);
        l ++;
        draw_n ( 5, cpun -> faultNumber, 4, l);
        l ++;
        draw_n (36, cpun -> subFault.bits, 4, l);

        usleep (10000);
      }

    // cleanup
    munmap (fbp, screensize);

    return 0;
  }

