#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#if defined(CONFIG_WIFI_CREDENTIALS)
#include <zephyr/net/wifi_mgmt.h>
#endif
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>

#include <stdlib.h>

#include "forte/forteinstance.h"
#include "forte/arch/forte_architecture.h"

LOG_MODULE_REGISTER(forte, LOG_LEVEL_DBG);

#ifndef K_FP_REGS
#define K_FP_REGS 0
#endif // K_FP_REGS

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4U];
static struct net_mgmt_event_callback mgmt_cb;
static struct net_dhcpv4_option_callback dhcp_cb;

static forte::C4diacFORTEInstance g4diacForteInstance;
static bool forte_started = false;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

void forte_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	LOG_INF("Starting Eclipse 4diac FORTE Runtime...");

	char progName[] = "forte";
	char *argv[] = { progName };
	if (auto result = forte::arch::CForteArchitecture::initialize(ARRAY_SIZE(argv), argv); result != 0) {
		LOG_ERR("Failed to initialize FORTE architecture: %d", result);
		return;
	}

	// Start FORTE listening on all interfaces, default port 61499
	if (!g4diacForteInstance.startupNewDevice("0.0.0.0:61499")) {
		LOG_ERR("Failed to start FORTE instance");
		forte::arch::CForteArchitecture::deinitialize();
		return;
	}

	LOG_INF("FORTE is running on port 61499. Ready for 4diac IDE connection.");
	g4diacForteInstance.awaitDeviceShutdown();

	forte::arch::CForteArchitecture::deinitialize();
}

static void handler(struct net_mgmt_event_callback *cb,
	uint64_t mgmt_event,
	struct net_if *iface)
{
	ARG_UNUSED(cb);
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type !=
			NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("  Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
			&iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
			buf, sizeof(buf)));
		LOG_INF("  Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
			&iface->config.ip.ipv4->unicast[i].netmask,
			buf, sizeof(buf)));
		LOG_INF("  Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
			&iface->config.ip.ipv4->gw,
			buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}

	if (forte_started) {
		return;
	}

	// Increased stack size slightly for C++ runtime overhead
	const size_t stackSize = 4096;
	k_thread_stack_t *stack = k_thread_stack_alloc(stackSize, 0);
	if (stack == nullptr) {
		LOG_ERR("Failed to allocate FORTE thread stack (%zu bytes)", stackSize);
		return;
	}
	auto *thread = static_cast<struct k_thread *>(k_malloc(sizeof(struct k_thread)));
	if (thread == nullptr) {
		LOG_ERR("Failed to allocate FORTE k_thread struct");
		k_thread_stack_free(stack);
		return;
	}
	forte_started = true;
	k_thread_create(thread, stack, stackSize, forte_fn, nullptr, nullptr, nullptr, 10, K_FP_REGS, K_NO_WAIT);
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			size_t length,
			enum net_dhcpv4_msg_type msg_type,
			struct net_if *iface)
{
	ARG_UNUSED(length);
	ARG_UNUSED(msg_type);
	ARG_UNUSED(iface);
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

extern "C" int main(void)
{
	LOG_INF("Run dhcpv4 client");

	net_mgmt_init_event_callback(&mgmt_cb, handler,
		NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
		DHCP_OPTION_NTP, ntp_server,
		sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, nullptr);

#if defined(CONFIG_WIFI_CREDENTIALS)
	struct net_if *wifi_iface = net_if_get_first_wifi();
	if (wifi_iface != nullptr) {
		int err = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, wifi_iface, nullptr, 0);
		if (err && err != -ENOENT) {
			LOG_WRN("Stored Wi-Fi auto-connect request returned %d", err);
		}
	}
#endif

	return 0;
}
