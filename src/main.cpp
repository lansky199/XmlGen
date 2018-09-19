#include "xmlgen.h"

int main(int argc, char** argv) {
	bool lastarg = false;
	std::string ifile = "gen.xml";
	for (int i = 1; i < argc; ++i) {
		lastarg = (i == (argc - 1));

		if (!strcmp(argv[i], "-f")) {
			if (lastarg) {
				printf("Invalid options.\n");
				return 0;
			}
			ifile = _strdup(argv[++i]);
		}
	}

	XmlGen gen;
	gen.generate(ifile);

#if _WIN32
	system("pause");
#endif
	return 0;
}