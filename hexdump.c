#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

void
hexdump (const unsigned char *ptr, int size, FILE *out)
/*
 * hex dump 'size' bytes starting at 'ptr'
 */
{
    const unsigned char *lptr = ptr;
    int count = 0;
    long start = 0;

    while (size-- > 0)
    {
	if ((count % 16) == 0)
	    fprintf (out, "\t%08lx:", start);
	fprintf (out, " %02x", *ptr++);
	count++;
	start++;
	if (size == 0)
	{
	    while ((count % 16) != 0)
	    {
		fprintf(out, "   ");
		count++;
	    }
	}
	if ((count % 16) == 0)
	{
	    fprintf (out, " ");
	    while (lptr < ptr)
	    {
	        unsigned char c = *lptr&0x7f;
		c = ((c < 32)||(c == 0x7f))?'.':c;
		fprintf (out, "%c", c);
		lptr++;
	    }
	    fprintf(out,"\n");
	}
    }
    if ((count % 16) != 0)
	fprintf(out, "\n");

    fflush (out);
    return;
}

