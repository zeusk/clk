/*
 * nbgen.c
 *
 *  Created on: Mar 22, 2011
 *      Author: cedesmith
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>

#define ARRAYSIZE( array ) ( sizeof( array ) / sizeof( array[0] ) )

struct NbgPart
{
	char fileName[256];
	void* data;
	int length;
	int start;
	int end;
};

struct NbgData
{
	char header1[0x800];
	char header2[0x800];
	struct NbgPart parts[16];
	int noParts;
};

struct PartEntry
{
	uint8_t BootInd;
	uint8_t FirstHead;
	uint8_t FirstSector;
	uint8_t FirstTrack;
	uint8_t FileSystem;
	uint8_t LastHead;
	uint8_t LastSector;
	uint8_t LastTrack;
	uint32_t StartSector;
	uint32_t TotalSectors;
};

struct part_def
{
	char name[32];					// partition name (identifier in mtd device)
	short size __attribute__ ((aligned(4)));	// size in blocks 1Mb = 8 Blocks
	bool asize __attribute__ ((aligned(4)));	// auto-size and use all available space 1=yes 0=no
};

struct vpartitions
{
	char tag[7] __attribute__ ((aligned(4)));
	struct part_def pdef[12];
	short extrom_enabled __attribute__ ((aligned(4)));
};

int					noParts;
struct vpartitions  vparts;
struct NbgData		data;

#define TAG_VPTABLE		"VPTABLE"
#define PART_DEFAULT	"misc=1,recovery=5,boot=5,system=150,cache=5,userdata=0"

int Blocks(size_t bytes)
{
	// 1 block = 64 sectors = 0x20000 bytes = 131072 bytes
	// 1 sector = 0x800 bytes = 2048 bytes
	return bytes / 0x20000 + ( ( bytes % 0x20000 ) != 0 ? 1 : 0 );
}

int BlocksToMB(size_t blocks)
{
	return ( blocks * 0x20000 ) / (1024 * 1024 );
}

int MBToBlocks(size_t size)
{
	return size * 1024 * 1024 / 0x20000;
}

int FillRecoveryPart(const char* recoveryFile)
{
	if ( recoveryFile == NULL )
		return 0;

	// Validate recovery file
	struct stat st;
	if ( stat( recoveryFile, &st ) != 0 )
	{
		fprintf( stderr, "%s recovery file not found\n", recoveryFile );
		return -1;
	}

	// Validate partition count
	if ( noParts >= ARRAYSIZE( vparts.pdef ) )
	{
		fprintf( stderr, "Too many partitions, max=%d\n", ARRAYSIZE( vparts.pdef ) );
		return -1;
	}

	int size = Blocks( st.st_size ); // Size in blocks

	// Set recovery partition, should be the first one, right after partition layout
	// in order to keep the NBH file at a minimum size
	strcpy( vparts.pdef[noParts].name, "recovery" );
	vparts.pdef[noParts].size = size;

	fprintf( stderr, "Recovery partition=%s, blocks=%d, size=%d MB\n", recoveryFile, size, BlocksToMB( size ) );

	noParts++;

	return 0;
}

int FillPartLayout(const char* partLayout)
{
	if ( partLayout == NULL )
	{
		fprintf( stderr, "Invalid partition layout\n" );
		return -1;
	}

	char buf[1024];
	strcpy( buf, partLayout );

	char *sname, *ssize;

	// Look for first partition
	sname = strtok( buf, "=" );
	ssize = strtok( NULL, "," ); 

	while ( sname != NULL && ssize != NULL )
	{
		int size = MBToBlocks( atoi( ssize ) );

		// Find partition index
		int idxPart;
		for ( idxPart = 0; idxPart < noParts; idxPart++ )
		{
			if ( strcmp( vparts.pdef[idxPart].name, sname ) == 0 )
				break;
		}

		// Make sure to not overlap partition array
		if ( idxPart >= ARRAYSIZE( vparts.pdef ) )
		{
			fprintf( stderr, "Too many partitions, max=%d\n", ARRAYSIZE( vparts.pdef ) );
			return -1;
		}

		// New partition?
		if ( idxPart >= noParts )
		{
			// Set new partition name and increment no of parts
			strcpy( vparts.pdef[idxPart].name, sname );
			noParts++;
		}

		// Partition data
		if ( vparts.pdef[idxPart].size < size )
			vparts.pdef[idxPart].size	= size;

		// Look for next partition
		sname = strtok( NULL, "=" );
		ssize = strtok( NULL, "," );
	}

	// Update variable partition flag and display table layout
	fprintf( stderr, "PTABLE layout:\n" );
	int varCount = 0;
	for ( int i = 0; i < noParts; i++ )
	{
		vparts.pdef[i].asize = ( vparts.pdef[i].size == 0 );
		varCount += ( vparts.pdef[i].asize != 0 );
		fprintf( stderr, "Partition=%s, blocks=%d, size=%d MB\n", vparts.pdef[i].name, 
			vparts.pdef[i].size, BlocksToMB( vparts.pdef[i].size ) );
	}

	// Validate number of variable partitions (max one allowed)
	if ( varCount > 1 )
	{
		fprintf( stderr, "Multiple variable partitions detected, count=%d\n", varCount );
		return -1;
	}

	return 0;
};

int AddPartImage(const char* fileName)
{
	if ( fileName == NULL )
	{
		fprintf( stderr, "Invalid image file name\n" );
		return -1;
	}

	// Validate image file
	struct stat st;
	if ( stat( fileName, &st ) != 0 )
	{
		fprintf( stderr, "%s image file not found\n", fileName );
		return -1;
	}

	// Make sure to not overwrite partition array
	if ( data.noParts >= ARRAYSIZE( data.parts ) )
	{
		fprintf( stderr, "Too many ROM partitions, max=%d\n", ARRAYSIZE( data.parts ) );
		return -1;
	}

	strcpy( data.parts[data.noParts].fileName, fileName );
	data.parts[data.noParts].data	= NULL;
	data.parts[data.noParts].length	= 0;
	data.parts[data.noParts].start	= ( data.noParts ? data.parts[data.noParts-1].end : 2 );				// in sectors (1blk = 64 sectors)
	data.parts[data.noParts].end	= Blocks( data.parts[data.noParts].start * 0x800 + st.st_size ) * 64;	// in sectors (1blk = 64 sectors)
	data.noParts++;

	return 0;
}

int AddPartData(void* pData, int length)
{
	if ( pData == NULL || length == 0 )
	{
		fprintf( stderr, "Invalid partition data\n" );
		return -1;
	}

	// Make sure to not overwrite partition array
	if ( data.noParts >= ARRAYSIZE( data.parts ) )
	{
		fprintf( stderr, "Too many ROM partitions, max=%d\n", ARRAYSIZE( data.parts ) );
		return -1;
	}

	data.parts[data.noParts].fileName[0] = 0;
	data.parts[data.noParts].data	= pData;
	data.parts[data.noParts].length	= length;
	data.parts[data.noParts].start	= ( data.noParts ? data.parts[data.noParts-1].end : 2 );			// in sectors (1blk = 64 sectors)
	data.parts[data.noParts].end	= Blocks( data.parts[data.noParts].start * 0x800 + length ) * 64;	// in sectors (1blk = 64 sectors)
	data.noParts++;

	return 0;
}

void PartSetCHS(struct PartEntry* part)
{
	uint32_t first	= part->StartSector;
	uint32_t last	= ( part->StartSector + part->TotalSectors - 1 );

	part->FirstHead		= (uint8_t) ( first % 0x40 & 0xFF );
	part->FirstTrack	= (uint8_t) ( first / 0x40 & 0xFF );
	part->FirstSector	= (uint8_t) ( ( ( ( ( first / 0x40 ) >> 8 ) << 6 ) & 0xFF ) + 1 );
	part->LastHead		= (uint8_t) ( last % 0x40 & 0xFF );
	part->LastTrack		= (uint8_t) ( last / 0x40 & 0xFF );
	part->LastSector	= (uint8_t) ( ( ( ( ( last / 0x40 ) >> 8 ) << 6 ) & 0xFF ) + 1 );
}

void WriteTag(uint32_t no, uint32_t tag, FILE* out)
{
	fwrite( &no, sizeof( no ), 1, out );
	fwrite( &tag, sizeof( tag ), 1, out );
}

void Save(const char* fileName, int nb)
{
	FILE* out = fopen( fileName, "w" );
	if ( out == NULL )
	{
		fprintf( stderr, "failed to open %s\n", fileName );
		exit( 20 );
	}

	uint32_t sectorNo = 0x0, tag = 0xFFFBFFFD;

	// Write file header block
	fwrite( data.header1, 1, 0x800, out );
	if ( nb ) WriteTag( sectorNo, tag, out );
	sectorNo++;

	// Write MSFLASH50 block
	fwrite( data.header2, 1, 0x800, out );
	if ( nb ) WriteTag( sectorNo, tag, out );
	sectorNo++;

	char sector[0x800];

	// Write partitions
	for ( int i = 0; i < data.noParts; i++ )
	{
		struct NbgPart* part = &data.parts[i];

		// Write empty sectors before part
		memset( sector, 0xFF, 0x800 );
		for ( int is = sectorNo; is < part->start; is++ )
		{
			fwrite( sector, 0x800, 1, out );
			if ( nb ) WriteTag( 0xFFFFFFFF, 0xFFFFFFFF, out );
			sectorNo++;
		}

		if ( strlen( part->fileName ) != 0 )
		{
			// Write part from file
			printf( "Writing %s\n", part->fileName );
			FILE* in = fopen( part->fileName, "r" );
			if ( in == NULL )
			{
				fprintf( stderr, "Failed to open %s\n", part->fileName );
				exit( 21 );
			}

			while( !feof( in ) )
			{
				memset( sector, 0xFF, 0x800 );
				fread( sector, 1, 0x800, in );
				fwrite( sector, 0x800, 1, out );

				if ( nb )
				{
					tag = 0xFFFFFFFF;
					for ( int ib = 0; ib < 0x800; ib++ )
					{
						if ( sector[ib] != 0xFF )
						{
							tag = ( i == 0 ? 0xFFFBFFFD : 0xFFFBFFFF);
							break;
						}
					}

					WriteTag( ( tag != 0xFFFFFFFF ) ? sectorNo : 0xFFFFFFFF, tag, out );
				}

				sectorNo++;
			}

			fclose( in );
		}
		else if ( data.parts[i].data && data.parts[i].length )
		{
			char* buf  = (char*) data.parts[i].data;
			int length = data.parts[i].length;

			while( length )
			{
				if ( length >= 0x800 )
				{
					memcpy( sector, buf, 0x800 );
					buf += 0x800;
					length -= 0x800;
				}
				else
				{
					memset( sector, 0xFF, 0x800 );
					memcpy( sector, buf, length );
					length = 0;
				}

				fwrite( sector, 0x800, 1, out );

				if ( nb )
				{
					tag = 0xFFFFFFFF;
					for ( int ib = 0; ib < 0x800; ib++ )
					{
						if ( sector[ib] != 0xFF )
						{
							tag = ( i == 0 ? 0xFFFBFFFD : 0xFFFBFFFF);
							break;
						}
					}

					WriteTag( ( tag != 0xFFFFFFFF ) ? sectorNo : 0xFFFFFFFF, tag, out );
				}

				sectorNo++;
			}
		}
		else
		{
			fprintf( stderr, "Invalid partition source\n" );
			exit( 22 );
		}

		// Write empty sectors at the end
		memset( sector, 0xFF, 0x800 );
		for ( int is = sectorNo; is < part->end; is++ )
		{
			fwrite( sector, 0x800, 1, out );
			if ( nb ) WriteTag( 0xFFFFFFFF, 0xFFFFFFFF, out );
			sectorNo++;
		}
	}

	fclose( out );
}

int main(int argc, char* argv[])
{
	printf( "nbgen v1.0 by cedesmith\n" );
	if ( argc < 2 )
	{
		fprintf( stderr, "	Usage:  -o:os.nb|os.payload\n"
						 "          -b:lk.bin\n"
						 "          -r:recovery.img\n"
						 "          -p:%s\n"
						 "          -e:on|off\n", PART_DEFAULT );
		return 1;
	}

	// parse arguments
	int nb = 0;
	short extRom = 0;
	const char* outFile = NULL;
	const char* bootFile = NULL;
	const char* recoveryFile = NULL;
	const char* partLayout = NULL;	

	for ( int i = 1; i < argc; i++ )
	{
		const char* buf = argv[i];
		if ( strlen( buf ) < 3 )
			continue;

		// parameter?
		if ( buf[0] != '-' || buf[2] != ':' )
			continue;

		// Extract command lower case
		char command = buf[1];
		if ( command >= 'A' && command <= 'Z' )
			command += ( 'a' - 'A' );

		// Parameter
		const char* param = buf + 3;

		switch( command )
		{
		case 'o':
			outFile = param;
			nb = strcasecmp( strrchr( param, '.' ), ".nb" ) == 0;
			break;

		case 'b':
			bootFile = param;
			break;

		case 'r':
			recoveryFile = param;
			break;

		case 'p':
			partLayout = param;
			break;

		case 'e':
			extRom = strcasecmp( param, "on" ) == 0;
			break;

		default:
			fprintf( stderr, "Unknown command=-%c, param=%s\n", buf[1], param );
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Create partition layout

	noParts = 0;
	memset( &vparts, 0, sizeof( vparts ) );

	strcpy( vparts.tag, TAG_VPTABLE );
	vparts.extrom_enabled = extRom;

	// Recovery partition
	if ( FillRecoveryPart( recoveryFile ) == -1 )
		return 2;

	// Fill partition layout (use default if not specified)
	if ( partLayout == NULL )
		partLayout = PART_DEFAULT;
	if ( FillPartLayout( partLayout ) == -1 )
		return 2;

	//////////////////////////////////////////////////////////////////////////
	// Create ROM partition header

	memset( &data, 0, sizeof( data ) );

	// cLK image
	if ( bootFile == NULL )
	{
		fprintf( stderr, "Boot (cLK) file name not found\n" );
		return 2;
	}
	if ( AddPartImage( bootFile ) == -1 )
		return 2;

	// Partition image
	fprintf( stderr, "PTABLE size=%d bytes\n", sizeof( vparts ) );
	if ( AddPartData( &vparts, sizeof( vparts ) ) == -1 )
		return 2;

	// Recovery image
	if ( recoveryFile != NULL && AddPartImage( recoveryFile ) == -1 )
		return 2;

	// Make file header
	memset( data.header1, 0, 0x800 );									// fill 1st sector with 00
	data.header1[0]		= 0xE9;											// fill signature bytes
	data.header1[1]		= 0xFD;
	data.header1[2]		= 0xFF;
	data.header1[512-2]	= 0x55;
	data.header1[512-1]	= 0xAA;

	// Write boot partition
	struct PartEntry* part = (struct PartEntry*)( data.header1 + 0x1BE );
	part->BootInd		= 0;
	part->FileSystem	= 0x23;	// XIP
	part->StartSector	= data.parts[0].start;
	part->TotalSectors	= ( data.parts[0].end - data.parts[0].start );
	PartSetCHS( part );

	/*
	part++;
	part->BootInd=0;
	part->FileSystem=0x25;	// IMGFS
	part->StartSector=data.parts[1].start;
	part->TotalSectors=(data.parts[1].end-data.parts[1].start);
	PartSetCHS(part);
	*/

	// Write MSFLASH50
	memset( data.header2, 0xFF, 0x800 );			// fill 2nd sector with FF
	memset( data.header2, 00, 0x64 );
	strcpy( data.header2, "MSFLSH50" );				// MSFLSH50 signature

	// Save
	Save( outFile, nb );

	return 0;
}
