spif-driver: Linux kernel module
================================

This repository contains a Linux kernel module to support spif operation.

The module has been compiled, installed and tested with Linux kernel `4.19.0-xilinx-v2019.2`. The kernel was generated with `PetaLinux 2019.2`. It has been tested on both 32- and 64-bit ARM platforms.

The driver has been tested on two different FPGA platforms:

- Trenz Electronic TE0715-04-15 board (ZYNQ 7000).
- Xilinx ZCU102 development board (ZYNQ Ultrascale+)


Compilation
-----------

`spif-driver` has been compiled with `Petalinux 2019.2`.

The following node must be added to the device tree:

```
/ {
    spif {
        compatible = "uom,spif";
        memory-region = <&spif_reserved>;
        dma0 = <&axi_dma_0 0>;
        apb = <&APB_M_0>;
    };
};
```

The `dma0` resource corresponds to the first event-processing pipe. An additional `dma` resource must be included for every additional pipe.

The driver expects to find 4 KB (per event-processing pipe) of reserved memory for its use. The reserved memory is platform-dependent. The following single-pipe device tree nodes have been used:

- Trenz Electronic TE0715-04-15 board (ZYNQ 7000).

```
/ {
    reserved-memory {
        #address-cells = <1>;
        #size-cells = <1>;
        ranges;

        spif_reserved: reserved: buffer@3ffff000 {
            no-map;
            reg = <0x3ffff000 0x1000>;
        };
    };
};
```

- Xilinx ZCU102 development board (ZYNQ Ultrascale+)

```
/ {
    reserved-memory {
        #address-cells = <2>;
        #size-cells = <2>;
        ranges;

        spif_reserved: buffer@7fef5000 {
            no-map;
            reg = <0x0 0x7fef5000 0x0 0x1000>;
        };
    };
};
```

The memory size must be adjusted for the number of event-processing pipes.


Installation
------------

The module is usually installed at `/lib/modules/$(KERNELRELEASE)/extra/`.

`spif-driver` can be added to `/etc/modules` for automatic loading at boot time.


Notes
-----

- `spif-driver` is an out-of-tree kernel module therefore its installation taints the kernel.
