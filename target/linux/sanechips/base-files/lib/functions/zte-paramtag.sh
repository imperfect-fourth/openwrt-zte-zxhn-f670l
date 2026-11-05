# SPDX-License-Identifier: GPL-2.0-only

. /lib/functions/system.sh

# Base MAC from the paramtag file in the stock parameter-tags JFFS2
zte_paramtag_base_mac() {
	local dir=/tmp/zte-paramtag hex mac

	mkdir -p $dir
	mount -t jffs2 -o ro mtd:parameter-tags $dir 2>/dev/null || {
		rmdir $dir
		return 1
	}
	hex=$(hexdump -v -e '1/1 "%02x"' $dir/paramtag 2>/dev/null)
	umount $dir
	rmdir $dir

	# MAC records: tag 0x0100 + n (LE), two length fields of 6, then the MAC
	case "$hex" in
	*000106000600????????????010106000600*) ;;
	*) return 1 ;;
	esac
	hex=${hex#*000106000600}
	mac=$(echo "$hex" | cut -c1-12 | sed 's/../&:/g; s/:$//')
	case "$mac" in
	00:00:00:00:00:00|ff:ff:ff:ff:ff:ff) return 1 ;;
	esac
	[ "$((0x${mac%%:*} & 1))" -eq 0 ] || return 1
	echo "$mac"
}
