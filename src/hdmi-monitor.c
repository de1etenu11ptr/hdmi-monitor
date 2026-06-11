#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define BUFFER_SIZE 4096

void handle_hdmi_connection()
{
	printf("drm device state changed\n");
	system("/usr/local/bin/hdmi-toggle");
}

int main(int argc, int argv)
{
	int fd;
	struct sockaddr_nl addr;
	char msg[BUFFER_SIZE];

	/* AF_NETLINK socket types are for communication between the kernel
	 * and the userspace */
	/* NETLINK_KOBJECT_UEVENT handles hardware device layer triggers (drm, usb, pci, and etc.) */
	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		perror("socket creation failed (are you root?)");
		return EXIT_FAILURE;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	/* group 1 is a multicast user group (groups of pids allowing collective msg
	 * operations instead of to a specific pid) allowing the connected program to
	 * receive and send msgs using the netlink communication protocol without a
	 * prior handshake (implemented in the netlink socket implmenetation.
	 * If sending msgs to any other group was needed, a handshake would need to be
	 * done first. (See `https://docs.kernel.org/driver-api/connector.html`) */
	/* handshakes can be done with `setsockopt()` */
	addr.nl_groups = 1;
	addr.nl_pid = getpid();

	/* See active connections to groups using `/proc/net/netlink` where `eth` is
	 * the AF_NETLINK socket protocol (11 being NETLINK_CONNECTOR), `pid` the process
	 * owning the socket, and `group` the multicast group it is connected to (which
	 * you can investigate in `/proc/<pid>/exe` */
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		fprintf(stderr, "failed to bind group %d to fd %d\n%s\n",
			addr.nl_groups, fd, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	printf("listening to kernel multiuser group %d through the socket %d\n",
	       addr.nl_groups, fd);

	while (1) {
		int msg_len = recv(fd, msg, sizeof(msg) - 1, 0);
		if (msg_len < 0) {
			perror("receive failed");
			break;
		}
		msg[msg_len] = '\0';

		char *ptr = msg;
		while (ptr < msg + msg_len) {
			/* you can use `udevadm monitor --system=drm` to view kernel event
			 * notifications */
			/* checks if it a notification for an event related to the drm
			 * (Direct Rendering Manager) system */
			if (strcmp(ptr, "SUBSYSTEM=drm") == 0) {
				handle_hdmi_connection();
			}
			ptr += strlen(ptr) + 1;
		}
	}

	close(fd);
	return 0;
}
