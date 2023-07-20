#define BREAD_INI_IMPLEMENTATION
#include "../../ini.h"

int cb(const char *section, const char *key, const char *value, void* userdata) {
	(void)userdata;
	printf("«%s».«%s» = «%s»\n", section, key, value);
	return 0;
}

int main(int argc, char *argv[]) {
	char *path = "test.ini";
	if (argc > 1) {
		path = argv[1];
	}
	FILE *f = fopen(path, "r");
	if (!f) return 1;
	int status = bread_parse_ini(f, NULL, cb);
	fclose(f);
	return status > 0 ? 0 : 2;
}
