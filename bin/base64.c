// base64(1)-like
#include "../base64.h"

#define BREAD_BASE64_IMPLEMENTATION
#include "../base64.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	int c, errflg = 0, dflag = 0;
	char *ifile = NULL, *ofile = NULL;
	while ((c = getopt(argc, argv, "DdEei:o:")) != -1) {
		switch (c) {
			case 'd':
			case 'D':
				dflag = 1;
				break;
			case 'e':
			case 'E':
				dflag = 0;
				break;
			case 'i':
				ifile = optarg;
				break;
			case 'o':
				ofile = optarg;
				break;
			case ':':
				fprintf(stderr,
						"Option -%c requires an operand\n", optopt);
				break;
			case '?':
				fprintf(stderr,
						"Unrecognized option: '-%c'\n", optopt);
				break;
		}
	}
	if (errflg) {
		fprintf(stderr, "usage:\t%s [-Dd] [-Ee] [-i infile] [-o outfile]\n"
						"\t-Dd\tdecode the input\n"
						"\t-Ee\tencode the input\n"
						"\t-i\tinput file (default: stdin)\n"
						"\t-o\toutput file (default: stdout)\n",
						argv[0]);
		return 2;
	}
	FILE *in = stdin, *out = stdout;
	if (ifile) in  = fopen(ifile, "r");
	if (ofile) out = fopen(ofile, "w");

	if (dflag) return bd64ss(out, in);
	else              be64ss(out, in);
	return 0;
}
