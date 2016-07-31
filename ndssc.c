#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#define VERSION "0.1"
#define NOSGBA_SRAM_SIZE 0x80000 // 512 KB

/* "NocashGbaBackupMediaSavDataFile" string */
const uint8_t id_str[31] = { 0x4E, 0x6F, 0x63, 0x61, 0x73, 0x68, 0x47, 0x62, 0x61, 0x42, 0x61, 0x63, 0x6B, 0x75, 0x70, 0x4D, 0x65, \
							 0x64, 0x69, 0x61, 0x53, 0x61, 0x76, 0x44, 0x61, 0x74, 0x61, 0x46, 0x69, 0x6C, 0x65 };

/* "SRAM" string */
const uint8_t sram_str[4] = { 0x53, 0x52, 0x41, 0x4D };

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("\n\tNo$GBA NDS Save Converter v%s - By DarkMatterCore\n", VERSION);
		printf("\tUsage: %s [infile] [outfile]\n\n", argv[0]);
		printf("\t- infile: Name of the input No$GBA SRAM save file.\n\n");
		printf("\t- outfile: Name of the output RAW SRAM save file.\n");
		printf("\n\tExample: %s \"Pokemon Diamond.sav\" \"diamond_raw.sav\"\n", argv[0]);
		return 1;
	}
	
	/* Open input file and read it to memory */
	FILE *nosgba_sav = fopen(argv[1], "rb");
	if (!nosgba_sav)
	{
		printf("\n\tError opening \"%s\" for reading.\n", argv[1]);
		return 1;
	}
	
	fseek(nosgba_sav, 0, SEEK_END);
	uint32_t fsize = ftell(nosgba_sav);
	rewind(nosgba_sav);
	
	uint8_t *nosgba_data = malloc(NOSGBA_SRAM_SIZE);
	if (!nosgba_data)
	{
		fclose(nosgba_sav);
		printf("\n\tError allocating memory buffer.\n");
		return 1;
	}
	
	memset(nosgba_data, 0, NOSGBA_SRAM_SIZE);
	fread(nosgba_data, ((fsize < NOSGBA_SRAM_SIZE) ? fsize : NOSGBA_SRAM_SIZE), 1, nosgba_sav);
	fclose(nosgba_sav);
	
	/* Check if this is actually a No$GBA SRAM save */
	if (memcmp(nosgba_data, id_str, 31) != 0 || memcmp(nosgba_data + 0x40, sram_str, 4) != 0)
	{
		printf("\n\tThis file is not a No$GBA (un)compressed SRAM save file!\n");
		return 1;
	}
	
	/* Get file compression status */
	bool is_compressed = (*((uint32_t*)(&(nosgba_data[0x44]))) == 1);
	
	/* If the file is compressed, this is the size of the packed data */
	/* Otherwise, it represents the size of the unpacked data */
	uint32_t size1 = *((uint32_t*)(&(nosgba_data[0x48])));
	
	/* If the file is compressed, this is the size of the unpacked data */
	/* Otherwise, it represents the start address of the unpacked data */
	uint32_t size2 = *((uint32_t*)(&(nosgba_data[0x4C])));
	
	printf("\n\tValid No$GBA %s save file.", (is_compressed ? "compressed" : "uncompressed"));
	printf("\n\tSize of %s data: %d bytes.", (is_compressed ? "packed" : "unpacked"), size1);
	
	if (is_compressed)
	{
		printf("\n\tSize of unpacked data: %d bytes.\n\n", size2);
	} else {
		printf("\n\tStart address of unpacked data: 0x%08x.\n\n", size2);
	}
	
	/* Create output file */
	FILE *raw_sav = fopen(argv[2], "wb");
	if (!raw_sav)
	{
		free(nosgba_data);
		printf("\tError opening \"%s\" for writing.\n", argv[2]);
		return 1;
	}
	
	bool del = false;
	
	if (is_compressed)
	{
		uint32_t i, j, k;
		for (i = 0; i < size1; i += j)
		{
			if (nosgba_data[0x50 + i] > 0)
			{
				if (nosgba_data[0x50 + i] == 0x80)
				{
					/* Copy nosgba_data[0x50 + i + 1] value 'n' times, where n = nosgba_data[0x50 + i + 2] */
					for (k = 0; k < *((uint16_t*)(&(nosgba_data[0x50 + i + 2]))); k++)
					{
						fwrite(&(nosgba_data[0x50 + i + 1]), 1, 1, raw_sav);
					}
					
					/* Skip next 4 bytes */
					j = 4;
				} else
				if (nosgba_data[0x50 + i] < 0x80)
				{
					/* Copy next nosgba_data[0x50 + i] values */
					fwrite(&(nosgba_data[0x50 + i + 1]), nosgba_data[0x50 + i], 1, raw_sav);
					
					/* Skip next (nosgba_data[0x50 + i] + 1) bytes */
					j = (nosgba_data[0x50 + i] + 1);
				} else {
					/* Copy nosgba_data[0x50 + i + 1] value 'n' times, where n = (nosgba_data[0x50 + i] - 0x80) */
					for (k = 0; k < (nosgba_data[0x50 + i] - 0x80); k++)
					{
						fwrite(&(nosgba_data[0x50 + i + 1]), 1, 1, raw_sav);
					}
					
					/* Skip next 2 bytes */
					j = 2;
				}
			} else {
				break;
			}
		}
		
		/* Safety check */
		if (ftell(raw_sav) != size2)
		{
			del = true;
			printf("\tOutput size mismatch! %d (raw_sav) != %d (size2)\n", ftell(raw_sav), size2);
		}
	} else {
		/* Copy size1 bytes from nosgba_data[size2] */
		fwrite(&(nosgba_data[size2]), size1, 1, raw_sav);
	}
	
	free(nosgba_data);
	fclose(raw_sav);
	
	if (del)
	{
		remove(argv[2]);
		return 1;
	}
	
	return 0;
}
