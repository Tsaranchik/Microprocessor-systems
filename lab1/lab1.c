#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define MMIO_BASE_ADDR 0x6001120000
#define MMIO_SIZE 0x10000

typedef struct {
	uint8_t cap_length;
	uint8_t reserved;
	uint16_t hciversion;
	uint32_t hcsparams1;
	uint32_t hcsparams2;
	uint32_t hcsparams3;
	uint32_t hccparams1;
	uint32_t hccparams2;
	uint32_t dboff;
	uint32_t rtsoff;
} xhci_cap_regs_t;


const char* get_speed_name(uint8_t speed_id)
{
	switch (speed_id) {
		case 0x0: return "Not Applicable";
		case 0x1: return "Full Speed (USB  1.1)";
		case 0x2: return "Low Speed (USB 1.1)";
		case 0x3: return "High Speed (USB 2.0)";
		case 0x4: return "SuperSpeed (USB 3.0)";
		case 0x5: return "SuperSpeed+ (USB 3.1)";
		case 0x6: return "SuperSpeed+ (USB 3.2)";
		default:  return "Reserved";
	}
}

const char* get_link_state_name(uint8_t link_state)
{
	switch (link_state) {
		case 0x0: return "U0 (Active)";
		case 0x1: return "U1 (Sleep)";
		case 0x2: return "U2 (Deeper Sleep)";
		case 0x3: return "U3 (Suspend)";
		case 0x4: return "Disabled";
		case 0x5: return "RxDetect (Waiting)";
		case 0x6: return "Inactive";
		case 0x7: return "Polling";
		case 0x8: return "Recovery";
		case 0x9: return "Hot Reset";
		case 0xA: return "Compliance Mode";
		case 0xB: return "Test Mode";
		case 0xC: return "Loopback";
		default:  return "Reserved";
	}
}

const char* get_protocol_name(uint8_t protocol) 
{
	switch (protocol) {
		case 0x00: return "Undefined";
		case 0x01: return "USB 2.0";
		case 0x02: return "USB 3.0";
		case 0x03: return "USB 3.1";
		default:   return "Unknown";
	}
}

const char* get_enhanced_protocol_info(uint32_t portsc, uint8_t port_speed)
{
	uint8_t port_protocol = (portsc >> 20) & 0xF;

	if (port_protocol != 0x00)
		return get_protocol_name(port_protocol);
	
	switch (port_speed) {
		case 0x1:
		case 0x2:
		case 0x3:
			return "USB 2.0";
		case 0x4:
			if (portsc & (1 << 25))
				return "USB 3.0";
			return "USB 3.x";
		case 0x5:
		case 0x6:
			return "USB 3.x";
		default:
			return "Unknown Protocol";
	}
}

void detect_usb3_ports(volatile void *mmio_base, uint8_t cap_length, int max_ports)
{
	printf("\n=== USB 3.x Port Detection ===\n");

	volatile uint32_t *port_regs = (volatile uint32_t *)((uintptr_t)mmio_base + cap_length + 0x400);
	int usb3_port_count = 0;

	for (int i = 0; i < max_ports; ++i) {
		uint32_t portsc = port_regs[i * 4];
		uint8_t port_speed = (portsc >> 10) & 0xF;

		if (portsc == 0x0a0002a0) {
			printf("Port %2d: Likely USB 3.x SuperSpeed port (signature: 0x0a0002a0)\n", i + 1);
			usb3_port_count++;
		} else if (port_speed >= 0x4 && port_speed <= 0x6) {
			printf("Port %2d: USB 3.x port detected by speed ID: 0x%x\n", i + 1, port_speed);
			usb3_port_count++;
		} else if (portsc & (1 << 25)) {
			printf("Port %2d: USB 3.0 port (bit 25 set)\n", i + 1);
			usb3_port_count++;
		}
	}
	printf("Total USB 3.x ports detected: %d\n", usb3_port_count);
}

void print_controller_info(volatile xhci_cap_regs_t *cap_regs)
{
	printf("=== xHCI Controller Information ===\n");
	printf("CAPLENGTH: 0x%02x (%d bytes)\n", cap_regs->cap_length, cap_regs->cap_length);
	printf("HCIVERSION: 0x%04x (xHCI %d.%d)\n",
	       cap_regs->hciversion,
	       (cap_regs->hciversion >> 8) & 0xFF,
	       cap_regs->hciversion & 0xFF);
	
	uint32_t hcsparams1 = cap_regs->hcsparams1;
	int max_slots = (hcsparams1 >> 0) & 0xFF;
	int max_intrs = (hcsparams1 >> 8) & 0x7FF;
	int max_ports = (hcsparams1 >>  24) & 0xFF;

	printf("\nHCSPARAMS1: 0x%08x\n", hcsparams1);
	printf("\tMax Device Slots: %d\n", max_slots);
	printf("\tMax Interrupts: %d\n", max_intrs);
	printf("\tMax Ports: %d\n", max_ports);

	uint32_t hcsparams2 = cap_regs->hcsparams2;
	int ist = (hcsparams2 >> 0) & 0xF;
	int erst_max = (hcsparams2 >> 4) & 0xF;
	int spc = (hcsparams2 >> 24) & 0xFF;

	printf("\nHCSPARAMS2: 0x%08x\n", hcsparams2);
	printf("\tIST: %d\n", ist);
	printf("\tERST Max: %d\n", erst_max);
	printf("\tSPR Capability: %d\n", spc);

	uint32_t hcsparams3 = cap_regs->hcsparams3;
	int u1_device_exit_latency = (hcsparams3 >> 0) & 0xFF;
	int u2_device_exit_latency = (hcsparams3 >> 16) & 0xFFFF;

	printf("\nHSCPARAMS3: 0x%08x\n", hcsparams3);
	printf("\tU1 Device Exit Latency: %d\n", u1_device_exit_latency);
	printf("\tU2 Device Exit Latency: %d\n", u2_device_exit_latency);

	uint32_t hccparams1 = cap_regs->hccparams1;
	
	printf("\nHCCPARAMS1: 0x%08x\n", hccparams1);
	printf("\t64-bit Addressing: %s\n", (hccparams1 & (1 << 0)) ? "Yes" : "No");
	printf("\tContext Size: %s\n", (hccparams1 & (1 << 2)) ? "64-byte" : "32-byte");
	printf("\tPort Power Control: %s\n", (hccparams1 & (1 << 4)) ? "Yes" : "No");
	printf("\tPort Indicators: %s\n", (hccparams1 & (1 << 5)) ? "Yes" : "No");
	printf("\tLight HC Reset: %s\n", (hccparams1 & (1 << 6)) ? "Yes" : "No");
	printf("\tLatency Tolerance: %s\n", (hccparams1 & (1 << 7)) ? "Yes" : "No");
	printf("\tNo Secondary SID: %s\n", (hccparams1 & (1 << 8)) ? "Yes" : "No");

	printf("\nDoorbell Offset: 0x%08x\n", cap_regs->dboff);
	printf("Runtime Register Offset: 0x%08x\n", cap_regs->rtsoff);
}

void print_operational_registers(volatile void *mmio_base, uint8_t cap_length)
{
	volatile uint32_t *op_regs = (volatile uint32_t *)((uintptr_t)mmio_base + cap_length);

	printf("\n=== Operational Registers ===\n");

	uint32_t usbcmd = op_regs[0];
	
	printf("USBCMD: 0x%08x\n", usbcmd);
	printf("\tRun/Stop: %s\n", (usbcmd & (1 << 0)) ? "Running" : "Stopped");
	printf("\tHost Controller Enable: %s\n", (usbcmd & (1 << 1)) ? "Yes" : "No");
	printf("\tInterrupter Enable: %s\n", (usbcmd & (1 << 2)) ? "Yes" : "No");
	printf("\tHost System Error Enable: %s\n", (usbcmd & (1 << 3)) ? "Yes" : "No");
	
	uint32_t usbsts = op_regs[1];
	
	printf("\nUSBSTS: 0x%08x\n", usbsts);
	printf("\tHCHalted: %s\n", (usbsts & (1 << 0)) ? "Yes" : "No");
	printf("\tHost System Error: %s\n", (usbsts & (1 << 2)) ? "Yes" : "No");
	printf("\tEvent Interrupt: %s\n", (usbsts & (1 << 3)) ? "Yes" : "No");
	printf("\tPort Change Detect: %s\n", (usbsts & (1 << 4)) ? "Yes" : "No");
	printf("\tController Not Ready: %s\n", (usbsts & (1 << 11)) ? "Yes" : "No");
	printf("\tHost Controller Error: %s\n", (usbsts & (1 << 12)) ? "Yes" : "No");

	uint32_t pagesize = op_regs[2];
	printf("\nPAGESIZE: 0x%08x (%d KB pages)\n", pagesize, (1 << (pagesize & 0xFF)) * 4);

	uint32_t config = op_regs[8];
	printf("\nCONFIG: 0x%08x (Max Device Slots Enabled: %d)\n", config, config & 0xFF);
}

void print_port_details(volatile void *mmio_base, uint8_t cap_length, int max_ports)
{
	volatile uint32_t *port_regs = (volatile uint32_t *)((uintptr_t)mmio_base + cap_length + 0x400);

	printf("\n === Detailed Port Information ===\n");
	printf("Total logical ports: %d\n\n", max_ports);
	printf("Port registers base: 0x%lx\n\n", (uintptr_t)port_regs);

	int external_ports = 0;
	int internal_ports = 0;
	int usb2_ports = 0;
	int usb3_ports = 0;
	int connected_ports = 0;
	int powered_ports = 0;

	for (int i = 0; i < max_ports; ++i) {
		uint32_t portsc = port_regs[i * 4];
		uint8_t port_speed = (portsc >> 10) & 0xF;
		uint8_t port_protocol = (portsc >> 20) & 0xF;
		uint8_t port_power = (portsc >> 9) & 0x1;
		uint8_t port_enabled = (portsc >> 1) & 0x1;
		uint8_t port_connected = (portsc >> 0) & 0x1;
		uint8_t port_reset = (portsc >> 4) & 0x1;
		uint8_t port_link_state = (portsc >> 5) & 0xF;

		printf("Port %2d:\n", i + 1);
		printf("\tRegister: 0x%08x\n", portsc);
		printf("\tConnected: %s\n", port_connected ? "Yes" : "No");
		printf("\tEnabled: %s\n", port_enabled ? "Yes" : "No");
		printf("\tPower: %s\n", port_power ? "On" : "Off");
		printf("\tReset: %s\n", port_reset ? "Activated" : "No");
		printf("\tSpeed: %s (ID: 0x%x)\n", get_speed_name(port_speed), port_speed);
		printf("\tProtocol: %s\n", get_enhanced_protocol_info(portsc, port_speed));
		printf("\tLink State: %s (ID: 0x%x)\n", get_link_state_name(port_link_state), port_link_state);

		if (port_connected) {
			connected_ports++;
			external_ports++;
		} else if (port_enabled || port_power) {
			internal_ports++;
		}

		if (port_power) powered_ports++;
		if (port_speed == 0x3) usb2_ports++;
		if (port_speed >= 0x4 && port_speed <= 0x6) usb3_ports++;

		printf("\t---\n");
	}

	printf("\n=== Ports Statistics ===\n");
	printf("Total ports: %d\n", max_ports);
	printf("Powered Ports: %d\n", powered_ports);
	printf("Connected Devices: %d\n", connected_ports);
	printf("External ports (with devices): %d\n", external_ports);
	printf("Internal ports (enabled): %d\n", internal_ports);
	printf("USB 2.0 ports: %d\n", usb2_ports);
	printf("USB 3.x ports: %d\n", usb3_ports);
	printf("Unused ports: %d\n", max_ports - external_ports - internal_ports);
}

void check_physical_ports_system()
{
	printf("\n=== Physical Ports (System Information) ===\n");

	printf("1. USB Device Tree:\n");
	system("lsusb -t 2>/dev/null | head -20");

	printf("\n2. USB Ports in /sys:\n");
	system("find /sys/bus/usb/devices/ -name \"*usb*\" -type d 2>/dev/null | grep -E \"[0-9]-[0-9]\" | sort | head -10");

	printf("\n3. USB Controller Info:\n");
	system("lspci -v -s 00:14.0 2>/dev/null | head -15");
}

int main()
{
	int fd;
	void *mmio_base;
	volatile xhci_cap_regs_t *cap_regs;

	printf("=== Full information about USB controller ===\n");

	fd = open("/sys/bus/pci/devices/0000:00:14.0/resource0", O_RDONLY | O_SYNC);
	if (fd == -1) {
		perror("Error while opening resource0");
		return 1;
	}

	mmio_base = mmap(NULL, MMIO_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (mmio_base == MAP_FAILED) {
		perror("mmap error");
		close(fd);
		return 1;
	}

	printf("Memory successfully mapped: %p\n\n", mmio_base);

	cap_regs = (volatile xhci_cap_regs_t *)mmio_base;
	int max_ports = (cap_regs->hcsparams1 >> 24) & 0xFF;

	print_controller_info(cap_regs);
	print_operational_registers(mmio_base, cap_regs->cap_length);
	print_port_details(mmio_base, cap_regs->cap_length, max_ports);
	detect_usb3_ports(mmio_base, cap_regs->cap_length, max_ports);
	check_physical_ports_system();

	munmap(mmio_base, MMIO_SIZE);
	close(fd);

	printf("=== Analysis Complete ===\n");
	
	return 0;
}
