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

#define TAG_INFO			"DEVINFO"
#define PART_DEFAULT		"recovery=6,boot=5,misc=1,system=150,userdata=0,cache=5"
#define PART_DEFAULT_EXT_ON	"recovery=6,boot=5,misc=1,system=150,userdata=0,cache=192!"
#define round(x) ((long)((x)+0.5))
const char* partLayout;

int noParts, noRec, noXtra;

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
}data;

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
	char name[32];								// partition name (identifier in mtd device)
	short size __attribute__ ((aligned(4)));	// size in blocks 1Mb = 8 Blocks
	bool asize __attribute__ ((aligned(4)));	// auto-size and use all available space 1=yes 0=no
};

struct vpartitions
{
	char tag[7] __attribute__ ((aligned(4)));
	struct part_def pdef[14];
	short extrom_enabled __attribute__ ((aligned(4)));
	short fill_bbt_at_start __attribute__ ((aligned(4)));
	short inverted_colors __attribute__ ((aligned(4)));
	short show_startup_info __attribute__ ((aligned(4)));
	short usb_detect __attribute__ ((aligned(4)));
	short cpu_freq __attribute__ ((aligned(4)));
	short boot_sel __attribute__ ((aligned(4)));
	short multi_boot_screen __attribute__ ((aligned(4)));
	short panel_brightness __attribute__ ((aligned(4)));
	short udc __attribute__ ((aligned(4)));
	short use_inbuilt_charging __attribute__ ((aligned(4)));
	short chg_threshold __attribute__ ((aligned(4)));
}vparts;

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

char* basename(const char* filename)
{
  char *p = strrchr (filename, '/');
  return p ? p + 1 : (char *) filename;
}

int FillLKPart(const char* lkFile)
{
	if ( lkFile == NULL )
		return 0;

	// Validate lk file
	struct stat st;
	if ( stat( lkFile, &st ) != 0 )
	{
		fprintf( stderr, "%s lk file not found\n", lkFile );
		return -1;
	}
	// Size in blocks (in most cases set to 3)
	// If we want to include new DEVINFO then we must add 1 block here
	// and make another change in Save()
	int lksize = (Blocks(st.st_size) < 4 ? 3 : Blocks(st.st_size)) /*+ 1*/;

	// Set lk partition first
	strcpy( vparts.pdef[0].name, "lk" );
	vparts.pdef[0].size = lksize;

	// Fix partLayout to include lk partition first
	char lkpart[4096];
	sprintf( lkpart, "lk=%d!,", lksize);
	strcat(lkpart, partLayout);
	partLayout = lkpart;

	return 0;
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

	// Find partition index
	int idxPart;
	for ( idxPart = 0; idxPart < noParts; idxPart++ )
	{
		if ( strcmp( vparts.pdef[idxPart].name, "recovery" ) == 0 ){
			noRec=idxPart;
			break;
		}
	}
		
	// Validate partition count
	if ( idxPart >= ARRAYSIZE( vparts.pdef ) )
	{
		fprintf( stderr, "Too many partitions, max=%d\n", ARRAYSIZE( vparts.pdef ) );
		return -1;
	}

	//int size = Blocks( st.st_size ); // Size in blocks
	int rsize = (1024*1024)*round((float)(st.st_size)/(1024*1024));
	// Set recovery partition, should be the first one, right after partition layout
	// in order to keep the NBH file at a minimum size
	strcpy( vparts.pdef[idxPart].name, "recovery" );
	if ( (vparts.pdef[idxPart].size*1024*1024) < rsize )
	{
		vparts.pdef[idxPart].size = Blocks( rsize );
	}

	fprintf( stderr, "[] recovery image:\n\t\t %s, blocks=%d, size=%d MB\n", recoveryFile, Blocks( rsize ), BlocksToMB( Blocks( rsize ) ) );

	return 0;
}

int FillXtraPart(const char* xtraFile)
{
	if ( xtraFile == NULL )
		return 0;

	// Validate file
	struct stat st;
	if ( stat( xtraFile, &st ) != 0 )
	{
		fprintf( stderr, "%s file not found\n", xtraFile );
		return -1;
	}
	
	// Find partition index
	int idxPart;
	for ( idxPart = 0; idxPart < noParts; idxPart++ )
	{
		if ( strcmp( vparts.pdef[idxPart].name, strndup( basename(xtraFile), strlen(basename(xtraFile))-4) ) == 0 ){
			noXtra=idxPart;
			break;
		}
	}
	
	// Validate partition count
	if ( idxPart >= ARRAYSIZE( vparts.pdef ) )
	{
		fprintf( stderr, "Too many partitions, max=%d\n", ARRAYSIZE( vparts.pdef ) );
		return -1;
	}

	strcpy( vparts.pdef[idxPart].name, strndup( basename(xtraFile), strlen(basename(xtraFile))-4) );
	// Handle Size
	int rsize = (1024*1024)*round((float)(st.st_size)/(1024*1024));
	if ( (vparts.pdef[idxPart].size*1024*1024) < rsize )
		vparts.pdef[idxPart].size = Blocks( rsize );

	fprintf( stderr, "[] %s image:\n\t\t %s, blocks=%d, size=%d MB\n", strndup( basename(xtraFile), strlen(basename(xtraFile))-4), xtraFile, Blocks( rsize ), BlocksToMB( Blocks( rsize ) ) );

	return 0;
}

int FillPartLayout(const char* Layout)
{
	if ( Layout == NULL )
	{
		fprintf( stderr, "Invalid partition layout\n" );
		return -1;
	}

	char buf[1024];
	strcpy( buf, Layout );

	char *sname, *ssize;

	// Look for first partition
	sname = strtok( buf, "=" );
	ssize = strtok( NULL, "," ); 

	while ( sname != NULL && ssize != NULL )
	{
		int size;
		if (strchr(ssize, '!') != NULL)
		{
			size = atoi(strndup( ssize, strlen(ssize)-1));
		}else{
			size = MBToBlocks( atoi( ssize ) );
		}
		
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
	fprintf( stderr, "[] PTABLE layout:\n" );
	int varCount = 0;
	for ( int i = 0; i < noParts; i++ )
	{
		vparts.pdef[i].asize = ( vparts.pdef[i].size == 0 );
		varCount += ( vparts.pdef[i].asize != 0 );
		fprintf( stderr, "\t\t %s, blocks=%d, size=%d MB\n", vparts.pdef[i].name, 
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

int AddPartImage(const char* fileName, int length)
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
	data.parts[data.noParts].start	= ( data.noParts ? data.parts[data.noParts-1].end : 2 );			// in sectors (1blk = 64 sectors)
	data.parts[data.noParts].end	= Blocks( data.parts[data.noParts].start * 0x800 + length ) * 64;	// in sectors (1blk = 64 sectors)
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
	// If we include a new DEVINFO then set uplim to (data.noParts)
	int uplim = (nb ? data.noParts : (data.noParts - 1));
	for ( int i = 0; i < uplim ; i++ )
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
			printf( "[%i] Writing %s\n", i, part->fileName );
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
						if ( sector[ib] != (char)0xFF )
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
			printf( "[%i] Writing %s\n", i, "DEVINFO" );
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
						if ( sector[ib] != (char)0xFF )
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

/* koko : Cosmetic change to output of app */
int main(int argc, char* argv[])
{
	printf( "=== nbgen v1.5: NB Generator\n" );
	printf( "=== Created by cedesmith - Optimized by xdmcdmc - Messed up by kokotas\n" );
	printf( "\n" );
	if ( argc < 2 )
	{
		fprintf( stderr, "   Usage: -o:os.nb\n"
						 "             create an os.nb to be used from any NBH generator\n"
						 "          -o:lk.img\n"
						 "             create an img file to be flashed through fastboot\n"
						 "          -b:lk.bin\n"
						 "             set the binary file of the lk\n"
						 "          -r:recovery.img\n"
						 "             set the img file of the recovery to be included\n"
						 "          -x:partition-name.img\n"
						 "             set the img file of the named partition to be included\n"
						 "          -p:%s\n"
						 "             you can enter the size in     MB: 'name=size' ,\n"
						 "                                 OR in blocks: 'name=size!'\n"
						 "          -e:on|off\n"
						 "             enable|disable ExtROM\n", PART_DEFAULT );
		return 1;
	}

	// parse arguments
	int nb = 0;
	short extRom = 0;
	const char* outFile = NULL;
	const char* bootFile = NULL;
	const char* recoveryFile = NULL;
	const char* xtraFile = NULL;
	partLayout = NULL;

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
			nb = (strcasecmp(strrchr(param, '.'), ".nb") == 0 ? 1 : 0);
			break;

		case 'b':
			bootFile = param;
			break;

		case 'r':
			recoveryFile = param;
			break;

		case 'x':
			xtraFile = param;
			break;

		case 'p':
			partLayout = param;
			break;

		case 'e':
			extRom = (strcasecmp( param, "on" ) == 0 ? 1 : 0);
			break;

		default:
			fprintf( stderr, "Unknown command=-%c, param=%s\n", buf[1], param );
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Create partition layout

	noParts = noRec = noXtra = 0;
	memset( &vparts, 0, sizeof( vparts ) );

	strcpy( vparts.tag, TAG_INFO );
	vparts.extrom_enabled = extRom;
	fprintf( stderr, "[] ExtROM:\n\t\t %s\n", (extRom == 0 ? "Disabled" : "Enabled") );
	vparts.fill_bbt_at_start = 0;
	vparts.inverted_colors = 0;
	vparts.show_startup_info = 0;
	vparts.usb_detect = 1;
	vparts.cpu_freq = 0;
	vparts.boot_sel = 0;
	vparts.multi_boot_screen = 0;
	vparts.panel_brightness = 0;
	vparts.udc = 0;
	vparts.use_inbuilt_charging = 0;
	vparts.chg_threshold = 0;
	
	if ( partLayout == NULL ){ // Use default partition layout if not specified through cmd
		if(vparts.extrom_enabled){
			partLayout = PART_DEFAULT_EXT_ON;		
		}else{
			partLayout = PART_DEFAULT;
		}
	}

	// LK partition
	if ( FillLKPart( bootFile ) == -1 )
		return 2;
	
	// Fill partition layout (use default if not specified)
	if ( FillPartLayout( partLayout ) == -1 )
		return 2;

	fprintf( stderr, "[] Partition num:\n\t\t %d\n", noParts );
	fprintf( stderr, "[] PTABLE size:\n\t\t %d bytes\n", sizeof( vparts ) );
	
	// If we include another image file
	if ( xtraFile != NULL )
	{
		if ( FillXtraPart( xtraFile ) == -1 )
			return 2;
	}
	
	// Recovery partition
	if ( recoveryFile != NULL )
	{
		if ( FillRecoveryPart( recoveryFile ) == -1 )
			return 2;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Create ROM partition header

	memset( &data, 0, sizeof( data ) );

	// cLK image
	if ( bootFile == NULL )
	{
		fprintf( stderr, "Boot (cLK) file name not found\n" );
		return 2;
	}
	if ( AddPartImage( bootFile , (vparts.pdef[0].size - 1) * 0x20000 ) == -1 )
		return 2;

	// Partition image
	if ( AddPartData( &vparts, sizeof( vparts ) ) == -1 )
		return 2;
		
	printf( "\n" );
	if ( noRec < noXtra ) // Recovery is before the extra partition
	{
		// Recovery image
		if ( recoveryFile != NULL )
		{
			if ( AddPartImage( recoveryFile , vparts.pdef[noRec].size * 0x20000 ) == -1 )
				return 2;
		}
		// Add another image
		if ( xtraFile != NULL )
		{
			if ( AddPartImage( xtraFile , vparts.pdef[noXtra].size * 0x20000 ) == -1 )
				return 2;
		}
	}else{
		// Add another image
		if ( xtraFile != NULL )
		{
			if ( AddPartImage( xtraFile , vparts.pdef[noXtra].size * 0x20000 ) == -1 )
				return 2;
		}
		// Recovery image
		if ( recoveryFile != NULL )
		{
			if ( AddPartImage( recoveryFile , vparts.pdef[noRec].size * 0x20000 ) == -1 )
				return 2;
		}
	}
		
	// Make file header
	memset( data.header1, 0, 0x800 );	// fill 1st sector with 00
	data.header1[0]		= 0xE9;		// fill signature bytes
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
	
	// Write MSFLASH50
	memset( data.header2, 0xFF, 0x800 );			// fill 2nd sector with FF
	memset( data.header2, 00, 0x64 );
	strcpy( data.header2, "MSFLSH50" );				// MSFLSH50 signature

	// Save
	Save( outFile, nb );
	printf( "\n" );
	printf( "Done!\n" );
	return 0;
}
