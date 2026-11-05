# OpenWrt for the ZTE ZXHN F670L

An [OpenWrt](https://github.com/openwrt/openwrt) fork with support for the
ZTE ZXHN F670L (ZX279128S SoC), built on the ZX279128S/H3600 port by
[Navid Ghahremani](https://github.com/ghahramani/openwrt).

**RAM boot only.** The image runs from RAM after a TFTP download from the
stock U-Boot. Nothing is written to flash, and a power cycle returns to the
stock firmware.

## Status

| Feature | State |
|---|---|
| Ethernet, 4 ports (lan1-3 LAN, lan4 WAN) | Works, both CPU cores |
| Hardware NAT offload (PPE) | Works: LuCI, Network, Firewall, Routing/NAT Offloading, Hardware |
| Wi-Fi 2.4 GHz (MT7603E) and 5 GHz (MT7663) | Works |
| LEDs | Work |
| Watchdog | Works |
| USB | Not tested |
| Buttons (reset, WPS, Wi-Fi) | Not tested |
| GPON, phone port | Not supported |
| Installation to flash | Not supported yet |

Each router's MAC addresses and Wi-Fi calibration are read at boot from its
own stock partitions (`parameter-tags`, `wlan`), mounted read-only. The
image contains no unit data.

## Hardware

| | |
|---|---|
| SoC | ZTE ZX279128S, 2x Cortex-A9 |
| RAM | 256 MiB (128 MiB usable from a RAM boot) |
| Flash | 256 MiB SPI NAND |
| Wi-Fi | MediaTek MT7603E (2.4 GHz), MT7663 (5 GHz), on PCIe |

## Build

On a Linux host with the [OpenWrt build prerequisites](https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem):

```sh
git clone -b f670l <this repository> openwrt
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
cp f670l.config .config
make defconfig
make menuconfig   # optional: add or remove packages
make -j$(nproc)
```

The first build also builds the toolchain and takes about an hour. The
image is
`bin/targets/sanechips/generic/openwrt-sanechips-generic-zte_zxhn-f670l-initramfs.uImage`.

In `make menuconfig`, `*` builds a package into the image and `M` builds it
as a separate package. Examples:

| Want | Select |
|---|---|
| QoS | `luci-app-sqm` |
| DNS over TLS / HTTPS | `stubby` / `https-dns-proxy`, `luci-app-https-dns-proxy` |
| WireGuard | `wireguard-tools`, `luci-proto-wireguard` |
| Dynamic DNS | `luci-app-ddns` |

About 118 MiB of RAM is free to the system, and a RAM-boot image keeps all
its packages in RAM, so keep the selection small.

## Boot

You need a 3.3 V USB-UART adapter and a TFTP server on `192.168.2.2`.

1. Connect the UART: the 4-pin header beside the top Ethernet transformer.
   Pin 1 (square) is VCC (leave unconnected), 2 TX, 3 RX, 4 GND; 115200 8N1.
2. Connect the PC to lan1 and put the image in the TFTP server's directory.
3. Power on and press any key except `1` within 3 s to stop at `=>`.
4. Run:
   ```
   setenv ipaddr 192.168.2.1; setenv serverip 192.168.2.2; setenv netmask 255.255.255.0
   tftp 0x44000000 openwrt-sanechips-generic-zte_zxhn-f670l-initramfs.uImage
   bootm 0x44000000
   ```
5. After about a minute, open http://192.168.2.1 (user `root`, no password).

To reboot back to U-Boot without a power cycle:

```sh
ubus call system watchdog '{"magicclose":false}'; ubus call system watchdog '{"stop":true}'
```

The watchdog resets the board about 30 s later.

## Credits

- [Navid Ghahremani](https://github.com/ghahramani/openwrt): the ZX279128S
  target, its drivers and the ZTE ZXHN H3600 support this port builds on.
- [OpenWrt](https://openwrt.org).

## License

GPL-2.0, like OpenWrt (see `COPYING`). Packages keep their own licenses.
