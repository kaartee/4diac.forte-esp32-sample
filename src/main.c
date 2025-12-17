#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(forte, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/fs/fs.h>

#include <stdlib.h>

// #include <c_interface/forte_c.h>

#ifndef K_FP_REGS
#define K_FP_REGS 0
#endif // K_FP_REGS


#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

#ifdef CONFIG_UPDATE_FORTE_BOOTFILE

#include <forte_fileio.h>

char* bootCmds =
";<Request ID=\"2\" Action=\"CREATE\"><FB Name=\"Blinky_RES\" Type=\"EMB_RES\" /></Request>\n"
"Blinky_RES;<Request ID=\"3\" Action=\"CREATE\"><FB Name=\"E_SWITCH\" Type=\"E_SWITCH\" /></Request>\n"
"Blinky_RES;<Request ID=\"4\" Action=\"CREATE\"><FB Name=\"E_SR\" Type=\"E_SR\" /></Request>\n"
"Blinky_RES;<Request ID=\"5\" Action=\"CREATE\"><FB Name=\"E_CYCLE\" Type=\"E_CYCLE\" /></Request>\n"
"Blinky_RES;<Request ID=\"6\" Action=\"WRITE\"><Connection Source=\"T#10ms\" Destination=\"E_CYCLE.DT\" /></Request>\n"
"Blinky_RES;<Request ID=\"7\" Action=\"CREATE\"><Connection Source=\"E_SWITCH.EO0\" Destination=\"E_SR.S\" /></Request>\n"
"Blinky_RES;<Request ID=\"8\" Action=\"CREATE\"><Connection Source=\"E_SWITCH.EO1\" Destination=\"E_SR.R\" /></Request>\n"
"Blinky_RES;<Request ID=\"9\" Action=\"CREATE\"><Connection Source=\"E_CYCLE.EO\" Destination=\"E_SWITCH.EI\" /></Request>\n"
"Blinky_RES;<Request ID=\"10\" Action=\"CREATE\"><Connection Source=\"START.COLD\" Destination=\"E_CYCLE.START\" /></Request>\n"
"Blinky_RES;<Request ID=\"11\" Action=\"CREATE\"><Connection Source=\"START.WARM\" Destination=\"E_CYCLE.START\" /></Request>\n"
"Blinky_RES;<Request ID=\"12\" Action=\"CREATE\"><Connection Source=\"START.STOP\" Destination=\"E_CYCLE.STOP\" /></Request>\n"
"Blinky_RES;<Request ID=\"13\" Action=\"CREATE\"><Connection Source=\"E_SR.Q\" Destination=\"E_SWITCH.G\" /></Request>\n"
"Blinky_RES;<Request ID=\"13\" Action=\"START\"/>\n"
;
#endif // CONFIG_UPDATE_FORTE_BOOTFILE

void forte_fn(void* arg1, void* arg2, void* arg3) {
//	char progName[] = "forte";
//	char flag[] = "-f";
//	char bootFile[] = "/lfs1/bootfile.txt";

#ifdef CONFIG_UPDATE_FORTE_BOOTFILE
	struct fs_file_t file = { 0 };
	fs_file_t_init(&file);
	if (0 == fs_open(&file, bootFile, FS_O_WRITE | FS_O_CREATE)) {
		fs_write(&file, bootCmds, strlen(bootCmds));
		fs_close(&file);
	}
#endif // CONFIG_UPDATE_FORTE_BOOTFILE

//	char* arguments[] = { progName, flag, bootFile };
//	const ssize_t argumentsCount = ARRAY_SIZE(arguments);
//	TForteInstance forteInstance = 0;
//	forteGlobalInitialize(argumentsCount, arguments);
//	int resultForte = forteStartInstanceGeneric(argumentsCount, arguments, &forteInstance);
//
//	if(FORTE_OK == resultForte) {
//		LOG_DBG("Started forte");
//		forteWaitForInstanceToStop(forteInstance);
//	} else {
//		LOG_DBG("Error %d: Couldn't start forte", resultForte);
//	}
//	forteGlobalDeinitialize();
}

static void handler(struct net_mgmt_event_callback *cb,
	uint64_t mgmt_event,
	struct net_if *iface)
{
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

	const size_t stackSize = 2048;
	k_thread_stack_t* stack = k_thread_stack_alloc(stackSize, 0);
	if (stack == NULL) return;
	struct k_thread* thread = k_malloc(sizeof(struct k_thread));
	if (thread == NULL) return;
	k_thread_create(thread, stack, stackSize, forte_fn, NULL, NULL, NULL, 10, K_FP_REGS, K_NO_WAIT);
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			size_t length,
			enum net_dhcpv4_msg_type msg_type,
			struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

int main(void)
{
	LOG_INF("Run dhcpv4 client");

	net_mgmt_init_event_callback(&mgmt_cb, handler,
		NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
		DHCP_OPTION_NTP, ntp_server,
		sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, NULL);

	return 0;
}

