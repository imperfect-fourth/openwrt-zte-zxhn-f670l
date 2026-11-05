# OpenWrt for the ZTE ZXHN F670L

An [OpenWrt](https://github.com/openwrt/openwrt) fork with support for the
ZTE ZXHN F670L (ZX279128S SoC), built on the ZX279128S/H3600 port by
[Navid Ghahremani](https://github.com/ghahramani/openwrt).

OpenWrt is first started from RAM over TFTP, then installed into the second
stock firmware bank. The stock firmware in the first bank, U-Boot and the
stock data partitions are kept.

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
| Installation to flash | Works, upgrades keep settings |

Each router's MAC addresses and Wi-Fi calibration are read at boot from its
own stock partitions (`parameter-tags`, `wlan`), mounted read-only. The
image contains no unit data.

## Hardware

| | |
|---|---|
| SoC | ZTE ZX279128S, 2x Cortex-A9 |
| RAM | 256 MiB (128 MiB usable) |
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
images are in `bin/targets/sanechips/generic/`:

| File | Use |
|---|---|
| `openwrt-sanechips-generic-zte_zxhn-f670l-initramfs.uImage` | RAM boot over TFTP |
| `openwrt-sanechips-generic-zte_zxhn-f670l-squashfs-sysupgrade.bin` | Installation and upgrades |

In `make menuconfig`, `*` builds a package into the image and `M` builds it
as a separate package. Examples:

| Want | Select |
|---|---|
| QoS | `luci-app-sqm` |
| DNS over TLS / HTTPS | `stubby` / `https-dns-proxy`, `luci-app-https-dns-proxy` |
| WireGuard | `wireguard-tools`, `luci-proto-wireguard` |
| Dynamic DNS | `luci-app-ddns` |

About 118 MiB of RAM is free to the system, so keep the selection small.

## RAM boot

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

## Install to flash

1. RAM-boot OpenWrt as above.
2. Copy the sysupgrade image to the router:
   ```sh
   scp -O openwrt-sanechips-generic-zte_zxhn-f670l-squashfs-sysupgrade.bin root@192.168.2.1:/tmp/
   ```
3. Install it:
   ```sh
   sysupgrade -n /tmp/openwrt-sanechips-generic-zte_zxhn-f670l-squashfs-sysupgrade.bin
   ```

The router reboots into OpenWrt from flash. The installation:

- replaces the second firmware bank (`kernel2`, 0x3a00000, 32 MiB) with the
  bank header and kernel (`kernel`) and a UBI root filesystem (`ubi`);
- sets `BootImageNum=0x00000001` in the `others` partition, so U-Boot boots
  that bank.

If the OpenWrt kernel cannot be loaded, U-Boot boots the stock firmware in
the first bank. UART and the RAM boot above always work for recovery.

Back up the whole flash first if you can; the second stock bank is lost.

## Upgrade

Use `sysupgrade` or LuCI (System, Backup / Flash Firmware) with the
sysupgrade image. Settings are kept.

## Back to stock (untested)

Make U-Boot boot the stock firmware in the first bank again:

```sh
dd if=/dev/mtd1 of=/tmp/others.bin bs=128k count=1
printf 'BootImageNum=0x00000000,' | dd of=/tmp/others.bin bs=24 count=1 conv=notrunc
mtd write /tmp/others.bin others
reboot
```

Check first that `/dev/mtd1` is `others` (`cat /proc/mtd`) and that
`/tmp/others.bin` starts with `BootImageNum=0x00000001,`.

## Credits

- [Navid Ghahremani](https://github.com/ghahramani/openwrt): the ZX279128S
  target, its drivers and the ZTE ZXHN H3600 support this port builds on.
- [OpenWrt](https://openwrt.org).

## License

GPL-2.0, like OpenWrt (see `COPYING`). Packages keep their own licenses.
