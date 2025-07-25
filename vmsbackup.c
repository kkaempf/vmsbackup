/*
 *
 *  Title:
 *	Backup
 *
 *  Description:
 *	Program to read VMS backup tape
 *
 *  Author:
 *	John Douglas CAREY.  (version 2.x, I think)
 *	Sven-Ove Westberg    (version 3.0)
 *      See NEWS file for more recent authors.
 *
 *  Net-addess (as of 1986 or so; it is highly unlikely these still work):
 *	john%monu1.oz@seismo.ARPA
 *	luthcad!sow@enea.UUCP
 *
 *
 *  Installation (again, as of 1986):
 *
 *	Computer Centre
 *	Monash University
 *	Wellington Road
 *	Clayton
 *	Victoria	3168
 *	AUSTRALIA
 *
 */

/* Does this system have the magnetic tape ioctls?  The answer is yes for 
   most/all unices, and I think it is yes for VMS 7.x with DECC 5.2 (needs
   verification), but it is no for VMS 6.2.  */
#ifndef HAVE_MT_IOCTLS
#define HAVE_MT_IOCTLS 1
#endif

#ifdef HAVE_UNIXIO_H
/* Declarations for read, write, etc.  */
#include <unixio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#if HAVE_MT_IOCTLS
#include <sys/ioctl.h>
#include <sys/mtio.h>
#endif
#ifdef REMOTE
#include <local/rmt.h>
#include <sys/stat.h>
#endif
#include <sys/file.h>

#include "fabdef.h"

#ifndef __vax
/* The help claims that mkdir is declared in stdlib.h but it doesn't
   seem to be true.  AXP/VMS 6.2, DECC ?.?.  On the other hand, VAX/VMS 6.2
   seems to declare it in a way which conflicts with this definition.
   This is starting to sound like a bad dream.  */
int mkdir (char *path, int mode);
#endif

#include "vmsbackup.h"
#include "match.h"
#include "sysdep.h"

#ifdef DEBUG
#include "hexdump.h"
static void debug_dump(const unsigned char* buffer, int dsize, unsigned int dtype, char *names[], int namecount)
{

	if (debugflag)
	{
	    char *name = "???";
	    if (dtype < namecount) {
	        name = names[dtype];
	    }
		printf(" dsize = 0x%x/%d dtype = 0x%x/%d:<%s> ",
			dsize, dsize, dtype, dtype, name);
/*		while(dsize--)
		{
			printf("%x/%d, ", *buffer, *buffer);
			buffer++;
		}
		printf("\n");
*/
	    if (dsize > 0)
		    hexdump(buffer, dsize, stdout);
	    else
		    printf("\n");
	}
}

static char *dtype_summary_names[] = {
/*          0,         1,         2,           3,            4        , 5,      6,    7 */
    "padding", "saveset", "command", "<unknown>", "written_by", "usr/grp", "date", "os",
/*            8,          9,        a,            b,                c,            d,            e,              f */
    "osversion", "nodename", "id_reg", "written_on", "backup_version", "block_size", "group_size", "buffer_count"
};

static char *dtype_names[] = {
/*       0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f */
    "data", "0x01", "0x02", "0x03", "0x04", "0x05", "0x06", "0x07",
    "0x08", "0x09", "0x0a", "0x0b", "0x0c", "0x0d", "0x0e", "0x0f",
    "0x10", "0x11", "0x12", "0x13", "0x14", "0x15", "0x16", "0x17",
    "0x18", "0x19", "0x1a", "0x1b", "0x1c", "0x1d", "0x1e", "0x1f",
    "0x20", "0x21", "0x22", "0x23", "0x24", "0x25", "0x26", "0x27",
    "0x28", "0x29", "filename", "0x2b", "fileid", "0x2d", "0x2e", "usr/grp",
    "protection", "0x31", "0x32", "0x33", "record format", "0x35", "date1", "date2",
    "expires", "date3"
};
#endif


/* Byte-swapping routines.  Note that these do not depend on the size
   of datatypes such as short, long, etc., nor do they require us to
   detect the endianness of the machine we are running on.  It is
   possible they should be macros for speed, but I'm not sure it is
   worth bothering.  We don't have signed versions although we could
   add them if needed.  They are, of course little-endian as that is
   the byteorder used by all integers in a BACKUP saveset.  */

unsigned long long getu64 (unsigned char *addr)
{
#ifdef DEBUG
//    if (debugflag) {
//	printf("getu64: ");
//	hexdump(addr, 8, stdout);
//    }
#endif
    unsigned long long u = 0LL;
    u |= (unsigned long long)addr[7] << 56;
    u |= (unsigned long long)addr[6] << 48;
    u |= (unsigned long long)addr[5] << 40;
    u |= (unsigned long long)addr[4] << 32;
    u |= (unsigned long long)addr[3] << 24;
    u |= (unsigned long long)addr[2] << 16;
    u |= (unsigned long long)addr[1] << 8;
    u |= (unsigned long long)addr[0];
    return u;
}

unsigned long getu32 (unsigned char *addr)
{
#ifdef DEBUG
//    if (debugflag) {
//	printf("getu32: ");
//	hexdump(addr, 4, stdout);
//    }
#endif
	return (((((unsigned long)addr[3] << 8) | addr[2]) << 8)
		| addr[1]) << 8 | addr[0];
}

unsigned int getu16 (unsigned char *addr)
{
#ifdef DEBUG
//    if (debugflag) {
//	printf("getu16: ");
//	hexdump(addr, 2, stdout);
//     }
#endif
	return ((unsigned int)addr[1] << 8) | addr[0];
}

struct bbh {
	unsigned char	bbh_dol_w_size[2];
	unsigned char	bbh_dol_w_opsys[2];
	unsigned char	bbh_dol_w_subsys[2];
	unsigned char	bbh_dol_w_applic[2];
	unsigned char	bbh_dol_l_number[4]; /* @8 */
	char	bbh_dol_t_spare_1[20];
	unsigned char	bbh_dol_w_struclev[2]; /* @32 0x20 */
	unsigned char	bbh_dol_w_volnum[2];
	unsigned char	bbh_dol_l_crc[4];
	unsigned char	bbh_dol_l_blocksize[4]; /* @40 0x28 */
	unsigned char	bbh_dol_l_flags[4];
	char	bbh_dol_t_ssname[32]; /* 1-byte length, followed by chars */
	unsigned char	bbh_dol_w_fid[3][2];
	unsigned char	bbh_dol_w_did[3][2];
	char	bbh_dol_t_filename[128];
	char	bbh_dol_b_rtype;
	char	bbh_dol_b_rattrib;
	unsigned char	bbh_dol_w_rsize[2];
	char	bbh_dol_b_bktsize;
	char	bbh_dol_b_vfcsize;
	unsigned char	bbh_dol_w_maxrec[2];
	unsigned char	bbh_dol_l_filesize[4];
	char	bbh_dol_t_spare_2[22];
	unsigned char	bbh_dol_w_checksum[2];
}__attribute__((packed, aligned(1))) *block_header; /* 256 bytes total */

struct brh {
	unsigned char	brh_dol_w_rsize[2];
	unsigned char	brh_dol_w_rtype[2];
	unsigned char	brh_dol_l_flags[4];
	unsigned char	brh_dol_l_address[4];
	unsigned char	brh_dol_l_spare[4];
}__attribute__((packed, aligned(1))) *record_header; /* 16 bytes total */

/* define record types */

#define	brh_dol_k_null	0
#define	brh_dol_k_summary	1
#define	brh_dol_k_volume	2
#define	brh_dol_k_file	3
#define	brh_dol_k_vbn	4
#define brh_dol_k_physvol	5
#define brh_dol_k_lbn	6
#define	brh_dol_k_fid	7
#ifdef	DEBUG
static char *brh_type_names[] = {
    "null", "summary", "volume", "file", "vbn", "physvol", "lbn", "fid"
};
#endif
struct bsa {
	unsigned char	bsa_dol_w_size[2];
	unsigned char	bsa_dol_w_type[2];
        char	        bsa_dol_t_text[1];
}__attribute__((packed, aligned(1))) *data_item;

#ifdef	STREAM
char	*def_tapefile = "/dev/rts8";
#else
char	*def_tapefile = "/dev/rmt8";
#endif

char	filename[128];
int	filesize;
int	afilesize;

char	recfmt;		/* record format */
char	recatt;		/* record attributes */

FILE	*f	= NULL;

/* Number of bytes we have read from the current file so far (or something
   like that; see process_vbn).  */
int	file_count;

short	reclen;
short	fix;
short	recsize;
int	vfcsize;

/* Number of files we have seen.  */
unsigned int nfiles;
/* Number of blocks in those files.  */
unsigned long nblocks;

#ifdef	NEWD
FILE	*lf;
#endif

int	fd;		/* tape file descriptor */

/* Command line stuff.  */

/* The save set that we are listing or extracting.  */
char	*tapefile;

/* We're going to say tflag indicates our best effort at the same
   output format as VMS BACKUP/LIST.  Previous versions of this
   program used tflag for a briefer output format; if we want that
   feature we'll have to figure out what option should control it.  */
int tflag;

int 	cflag, dflag, eflag, sflag, vflag, wflag, xflag, debugflag;

/* Extract files in binary mode.  FIXME: Haven't tried to document
   (here or in the manpage) exactly what that means.  I think it probably
   means (or should mean) to give us raw bits, the same as you would get
   if you read the file in VMS directly using SYS$QIO or some such.  But
   I haven't carefully looked at whether that is what it currently does.  */
int flag_binary;

/* More full listing (/FULL).  */
int flag_full;

/* Which save set are we reading?  */
int	selset;

/* These variables describe the files we will be operating on.  GARGV is
   a vector of GARGC elements, and the elements from GOPTIND to the end
   are the names.  */
char	**gargv;
int 	goptind, gargc;

int	setnr;

#define	LABEL_SIZE	80
char	label[LABEL_SIZE];

unsigned char	*block;
/* Default blocksize, as specified in -b option.  */
int	blocksize = 32256;

#if HAVE_MT_IOCTLS
struct	mtop	op;
#endif

static int typecmp(char *str);

FILE *openfile(char *fn)
{
	char	ufn[256];
	char	ans[80];
	char	*p, *q, s, *ext;
	int	procf;

	procf = 1;
	/* copy fn to ufn and convert to lower case */
	p = fn;
	q = ufn;
	while (*p) {
		if (isupper(*p))
			*q = *p - 'A' + 'a';
		else
			*q = *p;
		p++;
		q++;
	}
	*q = '\0';

	/* convert the VMS to UNIX and make the directory path */
	p = ufn;
	q = ++p;
	while (*q) {
		if (*q == '.' || *q == ']') {
			s = *q;
			*q = '\0';
			if(procf && dflag) mkdir(p, 0777);
			*q = '/';
			if (s == ']')
				break;
		}
		q++;
	}
	q++;
	if(!dflag) p=q;
	/* strip off the version number */
	while (*q && *q != ';') {
		if( *q == '.') ext = q;
		q++;
	}
	if (cflag) {
		*q = ':';
	}
	else {
		*q = '\0';
	}
	if(!eflag && procf) procf = typecmp(++ext);
	if(procf && wflag) {
		printf("extract %s [ny]",filename);
		fflush(stdout);
		fgets(ans, sizeof(ans), stdin);
		if(*ans != 'y') procf = 0;
	}
	if(procf)
		/* open the file for writing */
		return(fopen(p, "w"));
	else
		return(NULL);
}

static int typecmp(char *str)    /* Compare the filename type in str with our list
                   of file type to be ignored.  Return 0 if the
                   file is to be ignored, return 1 if the
                   file is not in our list and should not be ignored. */
{
        static char *type[] = {
                "exe",          /* vms executable image */
                "lib",          /* vms object library */
                "obj",          /* rsx object file */
                "odl",          /* rsx overlay description file */
                "olb",          /* rsx object library */
                "pmd",          /* rsx post mortem dump */
                "stb",          /* rsx symbol table */
                "sys",          /* rsx bootable system image */
                "tsk",          /* rsx executable image */
		"dir",          /* directory */
		"upd",
		"tlo",
		"tlb",          /* text library */
		"hlb",          /* help library */
                ""              /* null string terminates list */
        };
        register int    i;

        i = -1;
        while (*type[++i])
                if (strncmp(str, type[i],3) == 0)
                        return(0);      /* found a match, file to be ignored */
        return(1);                      /* no match found */
}

void process_summary (unsigned char *buffer, size_t rsize)
{
	size_t c;
	size_t dsize;
	unsigned char *text;
	unsigned int type;

	/* These are the various fields from the summary.  */
	unsigned long id = 0;
	char *saveset = "";
	size_t saveset_size = 0;
	char *command = "";
	size_t command_size = 0;
	char *written_by = "";
	size_t written_by_size = 0;
	unsigned short grp = 0377;
	unsigned short usr = 0377;
	char *os = "<unknown OS>"; /* '\0' terminated.  */
	char *osversion = "unknown";
	size_t osversion_size = 7;
	char *nodename = "";
	size_t nodename_size = 0;
	char *written_on = "";
	size_t written_on_size = 0;
	char *backup_version = "unknown";
	size_t backup_version_size = 7;
	char date[32] = "<unknown date>";
	short date_length = 0;
	unsigned long blksz = 0;
	unsigned int grpsz = 0;
	unsigned int bufcnt = 0;

	if (!tflag)
		return;
	/* check the header word */
	if (buffer[0] != 1 || buffer[1] != 1) {
		printf ("Cannot print summary; invalid data header\n");
		return;
	}
	c = 2;
	while (c < rsize) {
#ifdef DEBUG
	    if (debugflag) {
		hexdump(buffer + c, 16, stdout);
	    }
#endif
		dsize = getu16 ((unsigned char *)((struct bsa *)&buffer[c])->bsa_dol_w_size);
	        if (dsize < 0) {
		    fprintf(stderr, "Corrupted backup file, size is negative\n");
		    return;
		}

		type = getu16 ((unsigned char *)((struct bsa *)&buffer[c])->bsa_dol_w_type);
		text = (unsigned char *)((struct bsa *)&buffer[c])->bsa_dol_t_text;

		/* Probably should define constants for the cases in this
		   switch, but I don't know anything about whether they
		   have "official" names we should be using or anything
		   like that.  */
#ifdef DEBUG
		debug_dump((unsigned char *)text, dsize, type, dtype_summary_names, sizeof(dtype_summary_names)/sizeof(char *));
#endif
	        if (dsize > 0)
		switch (type) {
		case 0:
			/* This seems to be used for padding at the end
			   of the summary.  */
			goto end_of_summary;
		case 1:
			saveset = (char *)text;
			saveset_size = dsize;
			break;
		case 2:
			command = (char *)text;
			command_size = dsize;
			break;
		case 4:
			written_by = (char *)text;
			written_by_size = dsize;
			break;
		case 5:
			if (dsize == 4) {
				usr = getu16 (text);
				grp = getu16 (text + 2);
			}
			break;
		case 6:
			if (!(time_vms_to_asc (&date_length, date, text, dsize)
				 & 1))
			{
				strcpy (date, "error converting date");
				date_length = strlen (date);
			}
			break;
		case 7:
			if (dsize == 2) {
				unsigned short oscode;

				oscode = getu16 (text);
				switch(oscode)
				{
				case 0x800:
					os = "OpenVMS AXP";
					break;
				case 0x400:
					os = "OpenVMS VAX";
					break;
				case 0x004:
					os = "RSTS/E";
					break;
#ifdef DEBUG
				default:
					os = "Unknown OS Code";
					break;
#endif
				}
			}
			break;
		case 8:
			osversion = (char *)text;
			osversion_size = dsize;
			break;
		case 9:
			nodename = (char *)text;
			nodename_size = dsize;
			break;
		case 10:
			if (dsize >= 4) {
				id = getu32 (text);
			}
			break;
		case 11:
			written_on = (char *)text;
			written_on_size = dsize;
			break;
		case 12:
			backup_version = (char *)text;
			backup_version_size = dsize;
			break;
		case 13:
			if (dsize >= 4) {
				blksz = getu32 (text);
			}
			break;
		case 14:
			if (dsize >= 2) {
				grpsz = getu16 (text);
			}
			break;
		case 15:
			if (dsize >= 2) {
				bufcnt = getu16 (text);
			}
			break;
		default:
			/* I guess we'll silently ignore these, for future
			   expansion.  */
			break;
		}
		c += dsize + 4;
	}
  end_of_summary:
	printf ("Save set:          %.*s\n", (int)saveset_size, saveset);
	printf ("Written by:        %.*s\n", (int)written_by_size, written_by);
	printf ("UIC:               [%06o,%06o]\n", grp, usr);
	printf ("Date:              %.*s\n", date_length, date);
	printf ("Command:           %.*s\n", (int)command_size, command);
	printf ("Operating system:  %s version %.*s\n", os,
		(int)osversion_size, osversion);
	printf ("BACKUP version:    %.*s\n", (int)backup_version_size,
		backup_version);
	printf ("CPU ID register:   %08lx\n", id);
	printf ("Node name:         %.*s\n", (int)nodename_size, nodename);
	printf ("Written on:        %.*s\n", (int)written_on_size, written_on);
	printf ("Block size:        %lu\n", blksz);
	if (grpsz != 0)
		printf ("Group size:        %u\n", grpsz);
	printf ("Buffer count:      %u\n\n", bufcnt);
	/* The extra \n is to provide the blank line between the summary
	   and the list of files that follows.  */
}

void process_file(unsigned char *buffer, size_t rsize)
{
	int	i;
	unsigned char *p;
        char *q;
	long nblk;
	long ablk;
	short	dsize, lnch;
	short dtype;
	unsigned char *data;
	char *cfname;
	char *sfilename;
	char date1[32] = " <None specified>";
	char date2[32] = " <None specified>";
	char date3[32] = " <None specified>";
	char date4[32] = " <None specified>";
	short date_length = 0;
	unsigned int fileid1 = 0, fileid2 = 0, fileid3 = 0;
	unsigned int extension = 0;
	unsigned int protection = 0;

	/* Various other stuff extracted from the saveset.  */
	unsigned short grp = 0377;
	unsigned short usr = 0377;

	/* Number of blocks which should appear in output.  This doesn't
	   seem to always be the same as nblk.  */
	size_t blocks;
	size_t ablocks;

	int	c;

	int 	procf;

#ifdef DEBUG
    if (debugflag)
	    printf("process_file, expecting %ld bytes\n", rsize);
#endif
	/* check the header word */
	if (buffer[0] != 1 || buffer[1] != 1) {
		printf("Snark: invalid data header in process_file: %02x %02x\n", buffer[0], buffer[1]);
		exit(EXIT_FAILURE);
	}
	c = 2;
	while (c < rsize) {
		dsize = getu16 ((unsigned char *)((struct bsa *) &buffer[c])->bsa_dol_w_size);
		dtype = getu16 ((unsigned char *)((struct bsa *)&buffer[c])->bsa_dol_w_type);
		data = (unsigned char *)((struct bsa *)&buffer[c])->bsa_dol_t_text;

#ifdef DEBUG
		debug_dump(data, dsize, dtype, dtype_names, sizeof(dtype_names)/sizeof(char *));
#endif

		/* Probably should define constants for the cases in this
		   switch, but I don't know anything about whether they
		   have "official" names we should be using or anything
		   like that.  */
		switch (dtype) {
		case 0x2a:
			/* Copy the text into filename, and '\0'-terminate
			   it.  */
			p = data;
			q = filename;
			for (i = 0;
			     i < dsize && q < filename + sizeof (filename) - 1;
			     i++)
				*q++ = *p++;
			*q = '\0';
			break;
		case 0x2b:
			/* In my example, two bytes, 0x1 0x2.  */
			break;
		case 0x2c:
			/* In my example, 6 bytes,
			   0x7a 0x2 0x57 0x0 0x1 0x1.  */
			fileid1 = getu16(data);
			fileid2 = getu16(data + 2);
			fileid3 = getu16(data + 4);
			break;
		case 0x2e:
			/* In my example, 4 bytes, 0x00000004.  Maybe
			   the allocation.  */
			break;
		case 0x2f:
			if (dsize == 4) {
				usr = getu16 (data);
				grp = getu16 (data + 2);
			}
			break;
		case 0x34:
			recfmt = data[0];
			recatt = data[1];
			recsize = getu16 (&data[2]);
			/* bytes 4-7 unaccounted for.  */
			ablk = getu16 (&data[6]);
			nblk = getu16 (&data[10])
				/* Adding in the following amount is a
				   change that I brought over from
				   vmsbackup 3.1.  The comment there
				   said "subject to confirmation from
				   backup expert here" but I'll put it
				   in until someone complains.  */
				+ (64 * 1024) * getu16 (&data[8]);
			lnch = getu16 (&data[12]);
			/* byte 14 unaccounted for */
			vfcsize = data[15];
			if (vfcsize == 0)
				vfcsize = 2;
			/* bytes 16-31 unaccounted for */
			extension = getu16 (&data[18]);
			break;
		case 0x2d:
			/* In my example, 6 bytes.  hex 2b3c 2000 0000.  */
			break;
		case 0x30:
			/* In my example, 2 bytes.  0x44 0xee.  */
			protection = getu16 (&data[0]);
			break;
		case 0x31:
			/* In my example, 2 bytes.  hex 0000.  */
			break;
		case 0x32:
			/* In my example, 1 byte.  hex 00.  */
			break;
		case 0x33:
			/* In my example, 4 bytes.  hex 00 0000 00.  */
			break;
		case 0x4b:
			/* In my example, 2 bytes.  Hex 01 00.  */
			break;
		case 0x50:
			/* In my example, 1 byte.  hex 00.  */
			break;
		case 0x57:
			/* In my example, 1 byte.  hex 00.  */
			break;
		case 0x4f:
			/* In my example, 4 bytes.  05 0000 00.  */
			break;
		case 0x35:
			/* In my example, 2 bytes.  04 00.  */
			/* reviseno = getu16 ((char *)&data[0]); ! unused */
			break;
		case 0x36:
			/* In my example, 8 bytes.  Presumably a date.  */
			if (memcmp("\0\0\0\0\0\0\0\0", data, 8) != 0 &&
				!(time_vms_to_asc (&date_length, date4, data, 8)
				 & 1))
			{
				strcpy (date4, "error converting date");
			}
		        else if (debugflag)
			    printf("%s\n", date1);
			break;
		case 0x37:
			/* In my example, 8 bytes.  Presumably a date.  */
			if (memcmp("\0\0\0\0\0\0\0\0", data, 8) != 0 &&
				!(time_vms_to_asc (&date_length, date1, data, 8)
				 & 1))
			{
				strcpy (date1, "error converting date");
			}
		        else if (debugflag)
			    printf("%s\n", date1);
			break;
		case 0x38:
			/* In my example, 8 bytes.  Presumably expires
			   date, since my examples has all zeroes here
			   and BACKUP prints "<None specified>" for
			   expires.  */
			if (memcmp("\0\0\0\0\0\0\0\0", data, 8) != 0 &&
				!(time_vms_to_asc (&date_length, date2, data, 8)
				 & 1))
			{
				strcpy (date2, "error converting date");
			}
		        else if (debugflag)
			    printf("%s\n", date2);
			break;
		case 0x39:
			/* In my example, 8 bytes.  Presumably a date.  */
			if (memcmp("\0\0\0\0\0\0\0\0", data, 8) != 0 &&
				!(time_vms_to_asc (&date_length, date3, data, 8)
				 & 1))
			{
				strcpy (date3, "error converting date");
			}
		        else if (debugflag)
			    printf("%s\n", date3);
			break;
		case 0x47:
			/* In my example, 4 bytes.  01 00c6 00.  */
			break;
		case 0x48:
			/* In my example, 2 bytes.  88 aa.  */
			break;
		case 0x4a:
			/* In my example, 2 bytes.  01 00.  */
			break;
			/* then comes 0x0, offset 0x2b7.  */
		}
		c += dsize + 4;
	}

#ifdef	DEBUG
	if (debugflag)
	{
		printf("recfmt = %d\n", recfmt);
		printf("recatt = %d\n", recatt);
		printf("reclen = %d\n", recsize);
		printf("vfcsize = %d\n", vfcsize);
	}
#endif
	/* I believe that "512" here is a fixed constant which should not
	   depend on the device, the saveset, or anything like that.  */
	filesize = (nblk-1)*512 + lnch;
	blocks = (filesize + 511) / 512;
	afilesize = ablk*512;
	ablocks = ablk;
#ifdef DEBUG
	if (debugflag)
	{
		printf("nbk = %ld, abk = %ld, lnch = %d\n", nblk, ablk, lnch);
		printf("filesize = 0x%x, afilesize = 0x%x\n", filesize, afilesize);
	}
#endif

	/* open the file */
	if (f != NULL) {
		fclose(f);
		file_count = 0;
		reclen = 0;
	}
	procf = 0;
	if (goptind < gargc) {
		if (dflag) {
			cfname = filename;
		} else {
			cfname = strrchr(filename, ']') + 1;
		}
		sfilename = malloc (strlen (cfname) + 5);
		if (sfilename == NULL) {
			fprintf (stderr, "out of memory\n");
			exit (EXIT_FAILURE);
		}

		if (cflag) {
			for (i = 0; i < strlen(cfname); i++) {
				sfilename[i] = cfname[i];
				sfilename[i+1] = '\0';
			}
		} else {
			for (i = 0;
			     i < strlen(cfname) && cfname[i] != ';';
			     i++) {
				sfilename[i] = cfname[i];
				sfilename[i+1] = '\0';
			}
		}
		for (i = goptind; i < gargc; i++) {
			procf |= match (strlocase(sfilename),
					strlocase(gargv[i]));
		}
		free (sfilename);
	}
	else
		procf = 1;
	if (tflag && procf && !flag_full)
	    printf ("%-52s %8ld  %s\n", filename, blocks, date4);

	if (tflag && procf && flag_full) {
		printf ("%-30.30s File ID:  (%d,%d,%d)\n",
			filename,fileid1,fileid2,fileid3);
		printf ("  Size:       %6ld/%-6ld    Owner:    [%06o,%06o]\n",
			blocks,ablocks,grp, usr);
		printf ("  Protection: (");
		for (i = 0; i <= 3; i++)
		{
			printf("%c:", "SOGW"[i]);
			if (((protection >> (i * 4)) & 1) == 0)
			{
				printf("R");
			}
			if (((protection >> (i * 4)) & 2) == 0)
			{
				printf("W");
			}
			if (((protection >> (i * 4)) & 4) == 0)
			{
				printf("E");
			}
			if (((protection >> (i * 4)) & 8) == 0)
			{
				printf("D");
			}
			if (i != 3)
			{
				printf(",");
			}
		}
		printf(")\n");

#ifdef HAVE_STARLET
		printf("  Created:  %s\n", date4);
		printf("  Revised:  %s (%u)\n", date1, reviseno);
		printf("  Expires:  %s\n", date2);
		printf("  Backup:   %s\n", date3);
#endif

		printf ("  File Organization:  ");
		switch (recfmt & 0xf0)
		{
		case FAB$C_SEQ: printf("Sequential"); break;
		case FAB$C_REL: printf("Relative"); break;
		case FAB$C_IDX: printf("Indexed"); break;
		case FAB$C_HSH: printf("Hashed"); break;
		default: printf("<Unknown %d>", recfmt &0xf0); break;
		}
		printf("\n");

		printf("  File attributes:    Allocation %lu, Extend %d",
			ablocks, extension);
		printf("\n");
		printf ("  Record format:      ");
		switch (recfmt & 0x0f) {
		case FAB$C_UDF: printf ("(undefined)"); break;
		case FAB$C_FIX: printf ("Fixed length");
			if (recsize)
				printf (" %u byte records", recsize);
			break;
		case FAB$C_VAR: printf ("Variable length");
			if (recsize)
				printf (", maximum %u bytes", recsize);
			break;
		case FAB$C_VFC: printf ("VFC"); 
			if (recsize)
				printf (", maximum %u bytes", recsize);
			break;
		case FAB$C_STM: printf ("Stream"); break;
		case FAB$C_STMLF: printf ("Stream_LF"); break;
		case FAB$C_STMCR: printf ("Stream_CR"); break;
		default: printf ("<unknown>"); break;
		}
		printf ("\n");

		printf ("  Record attributes:  ");
		if (recatt & FAB$M_FTN) printf ("Fortran ");
		if (recatt & FAB$M_PRN) printf ("Print file ");
		if (recatt & FAB$M_CR) printf ("Carriage return carriage control ");
		if (recatt & FAB$M_BLK) printf ("Non-spanned");
		printf ("\n");
	}

	if (xflag && procf) {
		/* open file */
		f = openfile(filename);
		if(f != NULL && vflag) printf("extracting %s\n", filename);
	}
	++nfiles;
	nblocks += blocks;
}

/*
 *
 *  process a virtual block record (file record)
 *
 */
void process_vbn(unsigned char *buffer, unsigned short rsize)
{
	int	c, i;
	int j;

	if (f == NULL) {
		return;
	}
	i = 0;
	while (file_count+i < filesize && i < rsize) {
		switch (recfmt) {
		case FAB$C_FIX:
			if (reclen == 0) {
				reclen = recsize;
			}
			fputc(buffer[i], f);
			i++;
			reclen--;
			break;

		case FAB$C_VAR:
		case FAB$C_VFC:
			if (reclen == 0) {
				reclen = getu16 (&buffer[i]);
#ifdef	NEWD
				fprintf(lf, "---\n");
				fprintf(lf, "reclen = %d\n", reclen);
				fprintf(lf, "i = 0x%x/%d\n", i, i);
				fprintf(lf, "rsize = 0x%x/%d\n", rsize, rsize);
#endif
				fix = reclen;
				if (flag_binary) for (j = 0; j < 2; j++) {
					fputc (buffer[i+j], f);
				}
				i += 2;
				if (recfmt == FAB$C_VFC) {
					if (flag_binary)
						for (j = 0; j < vfcsize; j++) {
							fputc (buffer[i+j], f);
						}
					i += vfcsize;
					reclen -= vfcsize;
				}
			} else if (reclen == fix
					&& recatt == FAB$M_FTN) {
					/****
					if (buffer[i] == '0')
						fputc('\n', f);
					else if (buffer[i] == '1')
						fputc('\f', f);
					*** sow ***/
					fputc(buffer[i],f); /** sow **/
					i++;
					reclen--;
			} else {
				fputc(buffer[i], f);
				i++;
				reclen--;
			}
			if (reclen == 0) {
				if (!flag_binary) fputc('\n', f);
				if (i & 1) {
					if (flag_binary) fputc (buffer[i], f);
					i++;
				}
			}
			break;

		case FAB$C_STM:
		case FAB$C_STMLF:
			if (reclen < 0) {
				printf("SCREAM\n");
			}
			if (reclen == 0) {
				reclen = 512;
			}
			c = buffer[i++];
			reclen--;
			if (c == '\n') {
				reclen = 0;
			}
			fputc(c, f);
			break;

		case FAB$C_STMCR:
			c = buffer[i++];
			if (c == '\r' && !flag_binary)
				fputc('\n', f);
			else
				fputc(c, f);
			break;

		default:
			fclose(f); f = NULL;
			remove(filename);
			fprintf(stderr, "Invalid record format = %d\n", recfmt);
			return;
		}
	}
	file_count += i;
}

/*
 *
 *  process a backup block of blksize bytes
 *
 */
void process_block(unsigned char *block, int blksize)
{

	unsigned short	bhsize, rsize, rtype;
	unsigned long	bsize, i;
    unsigned long bnumber;

	i = 0;

	/* read the backup block header */
	block_header = (struct bbh *) &block[i];
	i += sizeof(struct bbh);

#ifdef	DEBUG
	if (debugflag) {
	    printf("get block header size and block size\n");
	    hexdump((unsigned char *)block_header, sizeof(struct bbh), stdout);
	}
#endif
	bhsize = getu16 ((unsigned char *)block_header->bbh_dol_w_size);
	bsize = getu32 ((unsigned char *)block_header->bbh_dol_l_blocksize);

	/* check the validity of the header block */
	if (bhsize != sizeof(struct bbh)) {
		fprintf (stderr,
			 "Invalid header block size: expected %ld got %d\n",
			 sizeof (struct bbh),
			 bhsize);
		exit(EXIT_FAILURE);
	}
	if (bsize != 0 && bsize != blksize) {
	    if (bsize > blksize) {
		printf("Snark: Block header blocksize too large, aborting\n");
		exit(EXIT_FAILURE);
	    }
	    printf("Detected changed blocksize, assuming Save Set\n");
	    blocksize = bsize;
	    vmsbackup();
	}

        bnumber = getu32((unsigned char *)block_header->bbh_dol_l_number);
#ifdef	DEBUG
	if (debugflag)
		printf("new block #%ld: i = %ld, bsize = 0x%lx/%ld\n", bnumber, i, bsize, bsize);
#endif
	/* read the records */
	while (i < (blksize - sizeof(struct brh))) {
		/* read the backup record header */
		record_header = (struct brh *) &block[i];
		i += sizeof(struct brh);

		rtype = getu16 ((unsigned char *)record_header->brh_dol_w_rtype);
		rsize = getu16 ((unsigned char *)record_header->brh_dol_w_rsize);
	    if (rsize > (bsize - i + 1)) {
		printf("Record size %d larger than remaining block size (%ld), aborting\n", rsize, (bsize-i));
		exit(EXIT_FAILURE);
	    }
#ifdef	DEBUG
	        if (debugflag)
		{
		    printf("Record header:\n");
		    hexdump((unsigned char *)record_header, sizeof(struct brh), stdout);
		    printf(" rtype = %d:%s\n", rtype, (rtype < sizeof(brh_type_names)/sizeof(char *))?brh_type_names[rtype]:"???");
			printf("  rsize = 0x%x/%d\n", rsize, rsize);
			printf("  flags = 0x%lx\n",
			       getu32 ((unsigned char *)record_header->brh_dol_l_flags));
			printf("  addr = 0x%lx\n",
			       getu32 ((unsigned char *)record_header->brh_dol_l_address));
			printf("  i = 0x%lx/%ld\n", i, i);
		}
#endif

		switch (rtype) {

		case brh_dol_k_null:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:null\n", rtype);
#endif
			/* This is the type used to pad to the end of
			   a block.  */
			break;

		case brh_dol_k_summary:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:summary\n", rtype);
#endif
			process_summary (&block[i], rsize);
			break;

		case brh_dol_k_file:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:file\n", rtype);
#endif
			process_file(&block[i], rsize);
			break;

		case brh_dol_k_vbn:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:vbn\n", rtype);
#endif
			process_vbn(&block[i], rsize);
			break;

		case brh_dol_k_physvol:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:physvol\n", rtype);
#endif
			break;

		case brh_dol_k_lbn:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:lbn\n", rtype);
#endif
			break;

		case brh_dol_k_fid:
#ifdef	DEBUG
			if (debugflag)
				printf("rtype = %d:fid\n", rtype);
#endif
			break;

		default:
			/* It is quite possible that we should just skip
			   this without even printing a warning.  */
#ifdef DEBUG
			if (debugflag)
			{
				fprintf (stderr,
					 " Warning: unrecognized record type\n");
				fprintf (stderr, " record type = %d, size = %d\n",
					 rtype, rsize);
			}
#endif
			break;
		}
#ifdef pyr
		i = i + rsize;
#else
		i += rsize;
#endif
#ifdef DEBUG
    if (debugflag) {
	printf("expecting next record, i at 0x%lx/%ld of 0x%x/%d\n", i, i, blksize, blksize);
    }
#endif
	}
    return;
}


int rdhead(void)
{
	int i, nfound;
	char name[80];
	nfound = 1;
#ifdef	DEBUG
    if (debugflag)
	    printf("rdhead\n");
#endif
	/* read the tape label - 4 records of 80 bytes */
	while ((i = read(fd, label, LABEL_SIZE)) != 0) {
		if (i != LABEL_SIZE) {
			fprintf(stderr, "Snark: bad label record\n");
			exit(EXIT_FAILURE);
		}
		if (strncmp(label, "VOL1",4) == 0) {
			sscanf(label+4, "%14s", name);
			if(vflag || tflag) printf("Volume: %s\n",name);
		}
		if (strncmp(label, "HDR1",4) == 0) {
			sscanf(label+4, "%14s", name);
			sscanf(label+31, "%4d", &setnr);
		}
		/* get the block size */
		if (strncmp(label, "HDR2", 4) == 0) {
			nfound = 0;
			sscanf(label+5, "%5d", &blocksize);
#ifdef	DEBUG
			if (debugflag)
				printf("\n\tblocksize = %d\n", blocksize);
#endif
		}
	}
	if((vflag || tflag) && !nfound) 
		printf("Saveset name: %s   number: %d\n",name,setnr);
	/* get the block buffer */
	block = (unsigned char *) malloc(blocksize);
	if (block == (unsigned char *) 0) {
		fprintf(stderr, "memory allocation for block failed\n");
		exit(EXIT_FAILURE);
	}
	return(nfound);
}

void rdtail(void)
{
	int i;
	char name[80];
	/* read the tape label - 4 records of 80 bytes */
	while ((i = read(fd, label, LABEL_SIZE)) != 0) {
		if (i != LABEL_SIZE) {
			fprintf(stderr, "Snark: bad label record\n");
			exit(EXIT_FAILURE);
		}
		if (strncmp(label, "EOF1",4) == 0) {
			sscanf(label+4, "%14s", name);
			if(vflag || tflag)
				printf("End of saveset: %s\n\n\n",name);
		}
	}
}

/* Perform the actual operation.  The way this works is that main () parses
   the arguments, sets up the global variables like cflags, and calls us.
   Does not return--it always calls exit ().  */
void vmsbackup(void)
{
	int	i, eoffl;

	/* Nonzero if we are reading from a saveset on disk (as
	   created by the /SAVE_SET qualifier to BACKUP) rather than from
	   a tape.  */
	int ondisk;

	if (tapefile == NULL)
		tapefile = def_tapefile;

#ifdef	NEWD
	/* open debug file */
	lf = fopen("log", "w");
	if (lf == NULL) {
		perror("log");
		exit(EXIT_FAILURE);
	}
#endif

	/* open the tape file */
	fd = open(tapefile, O_RDONLY);
	if (fd < 0) {
		perror(tapefile);
		exit(EXIT_FAILURE);
	}
#if HAVE_MT_IOCTLS
	/* rewind the tape */
	op.mt_op = MTREW;
	op.mt_count = 1;
	i = ioctl(fd, MTIOCTOP, &op);
	if (i < 0) {
		if (errno == EINVAL || errno == ENOTTY) {
			ondisk = 1;
		} else {
			perror(tapefile);
			exit(EXIT_FAILURE);
		}
	}
#else
	ondisk = 1;
#endif

#ifdef DEBUG
    if (debugflag) {
	printf("Opened '%s' as %s file: %d\n", tapefile, (ondisk?"disk":"tape"), fd);
    }
#endif
	if (ondisk) {
		/* process_block wants this to match the size which
		   backup writes into the header.  Should it care in
		   the ondisk case?  */
#ifdef FIXME
		/* This is already initialized in the variable definition,
		   and having this here makes the '-b' option a do-nothing
		   sort of thing, which makes it impossible to read
		   RSTS/E save sets */
		blocksize = 32256;
#endif
		block = malloc (blocksize);
		if (block == (unsigned char *) 0) {
			fprintf(stderr, "memory allocation for block failed\n");
			exit(EXIT_FAILURE);
		}
		eoffl = 0;
	} else {
		eoffl = rdhead();
	}

	nfiles = 0;
	nblocks = 0;

	/* read the backup tape blocks until end of tape */ 
	while (!eoffl) {
		if(sflag && setnr != selset) {
			if (ondisk) {
				fprintf(stderr, "-s not supported for disk savesets\n");
				exit(EXIT_FAILURE);
			}
#if HAVE_MT_IOCTLS
			op.mt_op = MTFSF;
			op.mt_count = 1;
			i = ioctl(fd, MTIOCTOP, &op);
			if (i < 0) {
				perror(tapefile);
				exit(EXIT_FAILURE);
			}
#else
			abort ();
#endif
			i = 0;
		}
		else
			i = read(fd, block, blocksize);
#ifdef DEBUG
    if (debugflag) {
	printf("Read %d of %d bytes, now at 0x%lx\n", i, blocksize, lseek(fd, 0, SEEK_CUR));
    }
#endif
		if(i == 0) {
			if (ondisk) {
				/* No need to support multiple save sets.  */
				eoffl = 1;
			} else {
				if (vflag || tflag)
					printf ("\nTotal of %u files, %lu blocks\n",
						nfiles, nblocks);
				rdtail();
				eoffl = rdhead();
			}
		}
		else if (i == -1) {
			perror ("error reading saveset");
			exit (EXIT_FAILURE);
		}
		else if (i != blocksize) {
			fprintf(stderr, "bad block read i = %d\n", i);
			exit(EXIT_FAILURE);
		}
		else {
			eoffl = 0;
			process_block(block, i);
		}
	}
	if(vflag || tflag) {
		if (ondisk) {
			printf ("\nTotal of %u files, %lu blocks\n",
				nfiles, nblocks);
			printf("End of save set\n");
		} else {
			printf("End of tape\n");
		}
	}

	/* close the tape */
	close(fd);

#ifdef	NEWD
	/* close debug file */
	fclose(lf);
#endif

	/* exit cleanly */
	exit(EXIT_SUCCESS);
}
