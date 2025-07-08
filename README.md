This is an example project that builds without modification for the ESP32 Ethernet kit boards, like WT32-ETH01, LILYGO T-INTERNET-POE, Espressif's ESP32-Ethernet-Kit.
Setup Zephyr, release X (temporarily, use the branch collab-sdk-0.18-dev), or a later release, according to the getting-started guide (https://docs.zephyrproject.org/latest/develop/getting_started/index.html).
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

The latest 4diac FORTE requires C++ 20 support is only available in the Zephyr 0.18 alpha SDK. Please set that up instead of the 0.17 series.

Also, please modify `zephyrproject/zephyr/include/zephyr/kernel.h`:

```
--- a/include/zephyr/kernel.h
+++ b/include/zephyr/kernel.h
@@ -6079,9 +6079,7 @@ struct k_poll_event {
 	.state = K_POLL_STATE_NOT_READY, \
 	.mode = _event_mode, \
 	.unused = 0, \
-	{ \
 		.typed_##_event_type = _event_obj, \
-	}, \
 	}
 
 #define K_POLL_EVENT_STATIC_INITIALIZER(_event_type, _event_mode, _event_obj, \
@@ -6092,9 +6090,7 @@ struct k_poll_event {
 	.state = K_POLL_STATE_NOT_READY, \
 	.mode = _event_mode, \
 	.unused = 0, \
-	{ \
 		.typed_##_event_type = _event_obj, \
-	}, \
 	}
 
 /**
```

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

## Building the firmware image

```
west build -p always -b esp32_ethernet_kit/esp32/procpu --sysbuild -s forte
```

For a completely clean rebuild, we recommend you to remove the build directories explicitly before building:

```
zephyrproject/build
zephyrproject/forte/lib/org.eclipse.4diac.forte/bin/
```

To download a 4diac IDE that matches the development branch of FORTE, you can find nightly development builds at:

`https://download.eclipse.org/4diac/updates/nightly/`

It is easiest to use if you download the full application archive and run the contained executable.

