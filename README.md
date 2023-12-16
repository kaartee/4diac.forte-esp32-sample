This is an example project that builds without modification for the ESP32 Ethernet kit boards, like WT32-ETH01, LILYGO T-INTERNET-POE, Espressif's ESP32-Ethernet-Kit.
Setup Zephyr according to the getting-started guide (https://docs.zephyrproject.org/latest/develop/getting_started/index.html).
Next, clone the example project, such that you have a standard directory layout for applications like this:

```
zephyrproject/
              zephyr/
              forte/    <- the sample application
```

Next:

```
west update
```

Depending on the type of the PHY on your ESP32 board, you may have to alter the RESET_N GPIO in `zephyrproject/zephyr/boards/xtensa/esp32_ethernet_kit/board_init.c`.
Boards, like the WT32-ETH01 with an RTL8720 PHY, use IO16 instead of IO5:

`#define IP101GRI_RESET_N_PIN	16 // 5`

## HW modification

The WT32-ETH01, at least until version 1.4, doesn't reset the PHY. This can result in a hanging PHY on occasion, which cannot
be remedied by a reset of the ESP32, but only by a full power cycle. The verified fix proposed here utilized the 50MHz oscillator-enable toggling
performed by the standard IDF implementation to the PHY reset line as well. This makes sure that resetting the ethernet library also
completely initializes the PHY as well.

In order to support full HW reset of the PHY, amend the WT32-ETH01 hardware according to the following steps:

Connect IO16_OSC_EN, hence EN of OSC50MHZ, to nRST of the RTL8720 PHY.

Steps:
- Completely remove R43
- Solder a lead from either pad connected to the trace between C18 and R43 to either pad at the trace between R50 and OSC50MHZ EN

## SW modification

Also, patch the partition table descriptor in `zephyrproject/modules/hal/espressif/components/partition_table/partitions_singleapp.csv` to accomodate a 2MB application size:

`factory,  app,  factory, ,        2M,`

Then:

```
west build -p always -b esp32_ethernet_kit forte
```

For a completely clean rebuild, we recommend you to remove the build directories explicitly before building:

```
zephyrproject/build
zephyrproject/forte/lib/org.eclipse.4diac.forte/bin/
```

To download a 4diac IDE that matches the development branch of FORTE, you can find nightly development builds at:

`https://download.eclipse.org/4diac/updates/nightly/`

It is easiest to use if you download the full application archive and run the contained executable.

