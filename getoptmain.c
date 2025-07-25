/* This is the version of main() for POSIX.2 (getopt) style arguments.  */

char *version = "VMSBACKUP4.3";

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include "vmsbackup.h"
#include "sysdep.h"

#ifdef HAVE_STARLET
#include "descrip.h"
#endif

static void usage (char *progname)
{
	fprintf (stderr, "Usage: %s -{tx}[cdevwFVBD][-b blocksize][-s setnumber][-f tapefile] [ name ... ]\n",
		 progname);
#ifdef HAVE_GETOPTLONG
	fprintf(stderr, "\nWith long versions of the above:\n"
	"\tb\tblocksize\tUse specified blocksize\n"
	"\tc\tcomplete\t\tRetain complete filename\n"
	"\td\tdirectory\tCreate subdirectories\n"
	"\te\textension\tExtract all files\n"
	"\tf\tfile\t\tRead from file\n"
	"\ts\tsaveset\t\tRead saveset number\n"
	"\tt\tlist\t\tList files in saveset\n"
	"\tv\tverbose\t\tList files as they are processed\n"
	"\tw\tconfirm\t\tConfirm files before restoring\n"
	"\tx\textract\t\tExtract files\n"
	"\tF\tfull\t\tFull detail in listing\n"
	"\tV\tversion\t\tShow program version number\n"
	"\tB\tbinary\t\tExtract as binary files\n"
	"\tD\tdebug\t\tPrint debug information (if compiled with DEBUG)\n"
	"\t?\thelp\t\tDisplay this help message\n");
#endif
}

extern int optind;
extern char *optarg;

#ifdef HAVE_GETOPTLONG
static const struct option OptionListLong[] =
{
	{"blocksize", 1, 0, 'b'},
	{"complete", 0, 0, 'c'},
	{"directory", 0, 0, 'd'},
	{"extension", 0, 0, 'e'},
	{"file", 1, 0, 'f'},
	{"saveset", 1, 0, 's'},
	{"list", 0, 0, 't'},
	{"verbose", 0, 0, 'v'},
	{"confirm", 0, 0, 'w'},
	{"extract", 0, 0, 'x'},
	{"full", 0, 0, 'F'},
	{"version", 0, 0, 'V'},
	{"binary", 0, 0, 'B'},
	{"debug", 0, 0, 'D'},
	{"help", 0, 0, '?'},
	{0, 0, 0, 0}
};
#endif

int main (int argc, char *argv[])
{
	char *progname;
	int c;
#ifdef HAVE_GETOPTLONG
	int OptionIndex;
#endif

	progname = argv[0];
	if(argc < 2){
		usage(progname);
		exit(1);
	}

	gargv = argv;
	gargc = argc;

	cflag=dflag=eflag=sflag=tflag=vflag=wflag=xflag=debugflag=0;
	flag_binary = 0;
	flag_full = 0;
	tapefile = NULL;

#ifdef HAVE_GETOPTLONG
	while((c=getopt_long(argc,argv,"b:cdef:s:tvwxFVBD",
		OptionListLong, &OptionIndex)) != EOF)
#else
	while((c=getopt(argc,argv,"b:cdef:s:tvwxFVBD")) != EOF)
#endif
		switch(c){
		case 'b':
			sscanf (optarg, "%d", &blocksize);
			break;
		case 'c':
			cflag++;
			break;
		case 'd':
			dflag++;
			break;
		case 'e':
			eflag++;
			break;
		case 'f':
			tapefile = optarg;
			break;
		case 's':
			sflag++;
			sscanf(optarg,"%d",&selset);
			break;
		case 't':
			tflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 'x':
			xflag++;
			break;
		case 'F':
			/* I'd actually rather have this be --full, but at
			   the moment I don't feel like worrying about
			   infrastructure for parsing long arguments.  I
			   don't like the GNU getopt_long--the interface
			   is noreentrant and generally silly; and it might
			   be nice to have something which synergizes more
			   closely with the VMS options (that one is a bit
			   of a can of worms, perhaps, though) like parseargs
			   or whatever it is called.  */
			flag_full = 1;
			break;
		case 'V':
			printf ("VMSBACKUP version %s\n", version);
			exit (EXIT_FAILURE);
			break;
		case 'B':
			/* This of course should be --binary; see above
			   about long options.  */
			flag_binary = 1;
			break;
		case 'D':
			/* Debugging code on */
			debugflag = 1;
			break;
		case '?':
			usage(progname);
			exit(1);
			break;
		};
	goptind = optind;
	if(!tflag && !xflag) {
		usage(progname);
		exit(1);
	}
	vmsbackup ();
    return 0;
}

/* The following is code for non-VMS systems which isn't related to main()
   or option parsing.  It should perhaps be part of a separate file
   (depending, of course, on things like whether anyone ever feels like
   separating the two concepts).  */

/* Given an 8-byte VMS-format date (little-endian) in SRCTIME, put an
   ASCII representation in *ASCBUFFER and put the length in *ASCLENGTH.
   ASCBUFFER must be big enough for 23 characters.
   Returns: condition code. (SS$_NORMAL == 1 for success)  */
#ifdef HAVE_STARLET
int time_vms_to_asc (short *asclength, char *ascbuffer, void *srctime, int srclen)
{
    struct dsc$descriptor buffer;

    if (srclen != 8)
      return SS$_INSFARG;
    buffer.dsc$w_length = 23;
    buffer.dsc$b_dtype = DSC$K_DTYPE_T;
    buffer.dsc$b_class = DSC$K_CLASS_S;
    buffer.dsc$a_pointer = ascbuffer;
    return sys$asctim (asclength, &buffer, srctime, 0);
}
#else
/* from vmsbackup.c */
unsigned long long getu64 (unsigned char *addr);
#include <time.h>
#include <string.h>
#include "hexdump.h"
/* See OpenVMS programming concepts manual volume 2, section 11.1
   "The time value is a binary number in 100-nanosecond (ns) units offset
    from the system base date and time, which is 00:00 o'clock,
    November 17, 1858 (the Smithsonian base date and time for the astronomic calendar).

    1 second = 10 million * 100 ns

    returns 1 for success
 */

int time_vms_to_asc (short *asclength, char *ascbuffer, void *srctime, int srclen)
{
    unsigned long long tval = 0;
    char *tstr;
    int tlen;
    if (srclen != sizeof(long long)) {
      errno = EINVAL;
      return 0;
    }
    /* get srctime as 64bit little endian to tval */
    tval = getu64(srctime);

//    printf("time_vms_to_asc %lld\n", tval);
//    hexdump((unsigned char *)&tval, 8, stdout);
    tval /= (10LL * 1000 * 1000); /* to seconds */
//    printf("time_vms_to_asc seconds %lld\n", tval);
    /* 17-Nov-1858 to 1-Jan-1970: 40587 days */
    tval -= (40587LL * 24 * 60 * 60); /* to epoch (1-Jan-1970) */
//    printf("time_vms_to_asc epoch %lld >%s<\n", tval, ctime((time_t *)&tval));
    tstr = ctime((time_t *)&tval);
    tlen = strlen(tstr);
    *(tstr+tlen-1) = 0; /* drop trailing newline */
//    printf("tstr >%s< %d\n", tstr, tlen);
    *asclength = tlen;
    strncpy(ascbuffer, tstr, 32);

    return 1;
}
#endif
