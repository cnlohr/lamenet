#include "librawp.h"
#include <stdio.h>
#include <netinet/if_ether.h>  //For ETH_P_ALL
#include <linux/if_packet.h> //For PACKET_ADD_MEMBERSHIP, etc.
#include <linux/wireless.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
//#include <net/if.h>


int librawp_setup( const char * interface, int channel )
{
	int sock_raw;
	struct ifreq ifr;
	struct sockaddr_ll sa;
	size_t if_name_len=strlen(interface);
	int ss = 0;

	sock_raw = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL) );
	if( sock_raw < 0 )
	{
		fprintf( stderr, "Error: Can't open socket.\n" );
		return -1;
	}
	if (if_name_len<sizeof(ifr.ifr_name)) {
		memcpy(ifr.ifr_name,interface,if_name_len);
		ifr.ifr_name[if_name_len]=0;
	
	}
	else
	{
		close( sock_raw );
		return -1;
	}

	ioctl( sock_raw, SIOCGIFINDEX, &ifr );

//#define PROMISC
#ifdef PROMISC
	ifr.ifr_flags |= IFF_PROMISC;
	ioctl(sock_raw, SIOCSIFFLAGS, &ifr);
#endif

	memset( &sa, 0, sizeof( sa ) );
	sa.sll_family=AF_PACKET;
	sa.sll_protocol = 0x0000;
	fprintf( stderr, "IFRI: %d\n", ifr.ifr_ifindex );
	sa.sll_ifindex = ifr.ifr_ifindex;
	sa.sll_hatype = 0;
//	sa.sll_pkttype = PACKET_HOST;
	bind( sock_raw,(const struct sockaddr *)&sa, sizeof( sa ) );


	// https://stackoverflow.com/questions/10070008/reading-from-pf-packet-sock-raw-with-read-misses-packets MAYBE?
	struct packet_mreq mr = { 0 };
	memset(&mr, 0, sizeof(mr));
	mr.mr_ifindex = sa.sll_ifindex;
	mr.mr_type = PACKET_MR_PROMISC;
	if (setsockopt(sock_raw, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0) {
		fprintf( stderr, "Error setting packet membership\n" );
	}


	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
	if( ioctl( sock_raw, SIOCGIFHWADDR, &ifr ) < 0 )
	{
		fprintf( stderr, "Error: Couldn't get self-MAC\n" );
		close( sock_raw );
		return -1;
	}

	//This one is really useful on some OLD systems.  On newer systems, it breaks things.
	ss = setsockopt( sock_raw, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface) );

	if( ss < 0 )
	{
		close( sock_raw );
		return -1;
	}



			setsockopt(sock_raw, SOL_SOCKET, SO_SNDBUFFORCE, &(int){1}, sizeof(int));
			setsockopt(sock_raw, SOL_SOCKET, SO_RCVBUFFORCE, &(int){1}, sizeof(int));
			setsockopt(sock_raw, SOL_SOCKET, SO_SNDBUF, &(int){1024*1024*10}, sizeof(int));
			setsockopt(sock_raw, SOL_SOCKET, SO_RCVBUF, &(int){1024*1024*10}, sizeof(int));

	return sock_raw;
}

int librawp_receive( int rawp, librawp_cb_t cb, void * id, int is_wifi )
{
	int data_size;
	uint8_t buffer[65536];

	while(1)
	{


//		data_size = recv( rawp, buffer, sizeof( buffer ), 0 );

		struct msghdr msg = { 0 };
		struct iovec iov[1] = { 0 };
		msg.msg_flags = 0;
		msg.msg_name = 0;
		msg.msg_namelen = 0;
		iov[0].iov_base = &buffer;
		iov[0].iov_len = sizeof( buffer );
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		data_size = recvmsg( rawp, &msg, 0 );

		if( data_size < 0 )
		{
			fprintf( stderr, "Recvfrom error, failed to get packets\n");
			return -2;
		}


		cb( id, 0, buffer, data_size );
	}

	return 0;
}


