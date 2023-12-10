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

Depending on the PHY on your ESP32 board, you may have to alter the RESET_N GPIO in `zephyrproject/zephyr/boards/xtensa/esp32_ethernet_kit/board_init.c`, the boards with an RTL8720 require:

`#define IP101GRI_RESET_N_PIN	16 // 5`

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
