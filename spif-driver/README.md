spif-driver: Linux kernel module
================================

This repository contains a Linux kernel module to support spif operation.

The module has been compiled, installed and tested with Linux kernel `4.19.0-xilinx-v2019.2`. The kernel was generated with `PetaLinux 2019.2`. It has been tested on both 32- and 64-bit ARM platforms.

The driver has been tested on two different FPGA platforms:

- Trenz Electronic TE0715-04-15 board (ZYNQ 7000).
- Xilinx ZCU102 development board (ZYNQ Ultrascale+)


Compilation
-----------

`spif-driver` has been compiled with `Petalinux 2019.2`. File `system-user.dtsi` was used to build the device tree during compilation for the Trenz Electronic TE0715-04-15 board (ZYNQ 7000). Equivalent updates to the device tree are required for other platforms.

The following node must be added to the device tree:

```
/ {
    spif {
        compatible = "uom,spif";
        apb = <&APB_M_0>;
        memory-region = <&spif_reserved>;
        dma0 = <&axi_dma_0 0>;
    };
};
```

The `dma0` resource corresponds to the first event-processing pipe. A `dma` resource must be included for every additional pipe.

The driver expects to find 4 KB (per event-processing pipe) of reserved memory for its use. The reserved memory is platform-dependent. The following single-pipe device tree node is used for this purpose:

```
/ {
    reserved-memory {
        #address-cells = <1>;
        #size-cells = <1>;
        ranges;

        spif_reserved: reserved: buffer@3fffc000 {
            compatible = "shared-dma-pool";
            reusable;
            reg = <0x3fffc000 0x4000>;
            linux,dma-default;
        };
    };
};
```

The memory size must be adjusted for the number of event-processing pipes.


Finally, support to drive the Ethernet RJ45 connector LEDs requires the following addtion:

<pre><code>
&gem0 {
    status = "okay";
    	ethernet_phy0: ethernet-phy@0 {
		compatible = "marvell,88e1510";
		device_type = "ethernet-phy";
        reg = <0>;
        <b>marvell,reg-init = <3 16 0xff00 0x12 3 17 0xfff0 0x5>;</b>
    };
};
</code></pre>

Installation
------------

The module is usually installed at `/lib/modules/$(KERNELRELEASE)/extra/`.

`spif-driver` can be added to `/etc/modules` for automatic loading at boot time.


Notes
-----

- `spif-driver` is an out-of-tree kernel module therefore its installation taints the kernel.
