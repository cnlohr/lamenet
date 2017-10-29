#include <stdio.h>
#include "librawp.h"
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <getopt.h>


int ifid1, ifid2;
volatile int c1, c2;

int PACKETPASS = 80; //Percent
int DELAYRAND = 25;
int ALLSTOPRAND = 1000;
int BASE_DELAY = 250;
int ADD_DELAY = 250;

uint8_t allstop = 0;


struct TXChain
{
	int size;
	int socket;
	uint8_t * data;
	uint32_t release_at_s;  //Time to release packet.
	uint32_t release_at_us;
	volatile struct TXChain * next;
};

struct TXChain * chain1;
struct TXChain * chain2;

FILE * pcaplog = 0;
const uint8_t pcap_hdr[] = {
	0xd4, 0xc3, 0xb2, 0xa1, 0x02, 0x00, 0x04, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };


void MakeLE32( uint32_t value, uint8_t * output )
{
	output[0] = value & 0xff;
	output[1] = (value>>8) & 0xff;
	output[2] = (value>>16) & 0xff;
	output[3] = (value>>24) & 0xff;
}

void * TXF( void * px )
{
	volatile struct TXChain * t = (struct TXChain*)px;
	while(1)
	{
		if( t->size )
		{
			struct timeval tv;
			while(1)
			{
				gettimeofday( &tv, 0 );
				if( tv.tv_sec > t->release_at_s || ( tv.tv_sec == t->release_at_s && tv.tv_usec >= t->release_at_us ) )
					break;
			}
			if( pcaplog )
			{
				uint8_t * data = t->data;
				int dlen = t->size;
				uint8_t hdr[16+dlen];
				MakeLE32( tv.tv_sec, &hdr[0] );
				MakeLE32( tv.tv_usec, &hdr[4] );
				MakeLE32( dlen, &hdr[8] );
				MakeLE32( dlen, &hdr[12] );
				memcpy( &hdr[16], data, dlen );
				fwrite( hdr, 16+dlen, 1, pcaplog );
			}

			send( t->socket, t->data, t->size, 0 );
		}
		while( !t->next || allstop ) usleep( 1000 );
		
		volatile struct TXChain * last = t;
		t = t->next;
		free( last->data );
		free( (struct TXChain *)last );
	}
}

void HandleSend( struct TXChain ** c, int socket, uint8_t * data, int length )
{
	struct timeval tv;
	struct TXChain * next = malloc( sizeof( struct TXChain ) );

	gettimeofday( &tv, 0 );

	next->size = length;
	next->socket = socket;
	next->data = malloc( length );
	memcpy( next->data, data, length );
	int wait = BASE_DELAY+ (rand()%ADD_DELAY);
	next->release_at_us = tv.tv_usec + wait*1000;
	next->release_at_s = tv.tv_sec + next->release_at_us/1000000;
	next->release_at_us %= 1000000;
	next->next = 0;

	(*c)->next = next;
	(*c) = next;
}

void rxfn1( void * id, void * rr, uint8_t * data, int dlen )
{
	c1++;
	while( allstop ) usleep(1000);
	if( ( rand()%100 ) > PACKETPASS ) return;
	usleep( 1000*(rand()%DELAYRAND) );

	HandleSend( &chain2, ifid2, data, dlen );

}

void rxfn2( void * id, void * rr, uint8_t * data, int dlen )
{
	c2++;
	if( ( rand()%100 ) > PACKETPASS ) return;
	while( allstop ) usleep(1000);
	usleep( 1000*(rand()%DELAYRAND) );
	HandleSend( &chain1, ifid1, data, dlen );
}

void * starter( void * v )
{
		librawp_receive( ifid2, rxfn2, 0, 0 );
}
void * watcher( void * v )
{
	while(1)
	{
		fprintf( stderr, "%d/%d\n", c1, c2 );

		fflush( stdout );
		allstop = 1;
		usleep( 1000 * (rand()%ALLSTOPRAND) );
		allstop = 0;
		sleep(1);
	}
}

int main( int argc, char ** argv )
{
	const char * logfile = 0;
	if( argc < 3 )
	{
		goto error_message;
	}
	fprintf( stderr, "Starting\n" );
	ifid1 = librawp_setup( argv[1], 0 );
	ifid2 = librawp_setup( argv[2], 0 );
	fprintf( stderr, "Librawp: %d/%d\n", ifid1, ifid2 );
	if( ifid1 < 0 )
	{
		fprintf( stderr, "Error opening interface.\n" );
		return -5;
	}
	if( ifid2 < 0 )
	{
		fprintf( stderr, "Error opening interface.\n" );
		return -5;
	}

	int c;
	while( (c = getopt( argc+2, argv+2, "p:d:a:b:t:l:?" )) != -1 )
	{
		switch( c )
		{
		case 'p': PACKETPASS = atoi( optarg ); break;
		case 'd': DELAYRAND = atoi( optarg ); break;
		case 'a': ALLSTOPRAND = atoi( optarg ); break;
		case 'b': BASE_DELAY = atoi( optarg ); break;
		case 't': ADD_DELAY = atoi( optarg ); break;
		case 'l': logfile = optarg; break;
		case '?': case 'h':
		default:
			goto error_message;
		}
	}

	if( logfile )
	{
		if( strcmp( logfile, "-" ) == 0 )
		{
			pcaplog = stdout;
			fprintf( stderr, "Logging to standard out.\n" );
		}
		else
			pcaplog = fopen( logfile, "wb" );
		fwrite( pcap_hdr, sizeof( pcap_hdr ), 1, pcaplog );
	}

	fprintf( stderr, "Bound interfaces: %s %s\n", argv[1], argv[2] );
	fprintf( stderr, "Packet pass percent: %d\n", PACKETPASS );
	fprintf( stderr, "Blocking Random Time: %d\n", DELAYRAND );
	fprintf( stderr, "All Data Stop Time (rand): %d\n", ALLSTOPRAND );
	fprintf( stderr, "Base Delay: %d\n", BASE_DELAY );
	fprintf( stderr, "Additional Random Delay: %d\n", ADD_DELAY );


	chain1 = malloc( sizeof( struct TXChain ) );
	chain2 = malloc( sizeof( struct TXChain ) );
	memset( chain1, 0, sizeof( struct TXChain ) );
	memset( chain2, 0, sizeof( struct TXChain ) );
	pthread_t x1, x2;
	pthread_create( &x1, 0, TXF, chain1 );
	pthread_create( &x2, 0, TXF, chain2 );

	pthread_t t;
	pthread_create( &t, 0, starter, 0 );
	pthread_t t2;
	pthread_create( &t2, 0, watcher, 0 );

	librawp_receive( ifid1, rxfn1, 0, 0 );

	fprintf( stderr, "Error: capture closed prematurely.\n" );
	return 0;

error_message:
	fprintf( stderr, "Error: Usage: lamenet [interface1] [interface2] [additional options]\n" );
	fprintf( stderr, " Options:\n  -p [percent pass]\n  -d [per-packet blocking time in ms]\n  -a [all stop time to happen every second (in ms)]\n  -b [base, per-packet delay in ms]\n  -t [additional random delay time per packet in ms]\n  -l [pcap file for logging]\n" );
	return -1;
}
