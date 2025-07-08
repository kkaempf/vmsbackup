/* match.h */

typedef int BOOLEAN;
#define VOID void
#define TRUE 1
#define FALSE 0
#define EOS '\000'

BOOLEAN match (char *string, char *pattern);
char *strlocase(char *str);
