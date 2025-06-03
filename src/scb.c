// SCB: @output(scb)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#define DEFAULT_CC "gcc"
#define DEFAULT_LD "gcc"
#elif defined(_WIN32)
#define DEFAULT_CC "gcc"
#define DEFAULT_LD "gcc"
#elif defined(__APPLE__)
#define DEFAULT_CC "gcc"
#define DEFAULT_LD "gcc"
#else
#error "Unsupported OS"
#endif

typedef struct SCB_FileConfig {
	char *filepath;
	char *cc;
	char *cflags;
	char *ldflags;
} SCB_FileConfig;

typedef struct SCB_GlobalConfig {
	char *output;
	char **sourcePaths;
	SCB_FileConfig **sources;
	int sourceCount;
	char *cc;
	char *ld;
	char *cflags;
	char *ldflags;
	int dryRun;
} SCB_GlobalConfig;

typedef enum SCB_Platform {
	PLATFORM_LINUX,
	PLATFORM_MACOS,
	PLATFORM_WINDOWS,
	PLATFORM_UNKNOWN
} SCB_Platform;

SCB_GlobalConfig GlobalConfig = {0};

SCB_Platform SCB_GetCurrentPlatform(void) {
#ifdef __linux__
	return PLATFORM_LINUX;
#elif defined(_WIN32)
	return PLATFORM_WINDOWS;
#elif defined(__APPLE__)
	return PLATFORM_MACOS;
#else
	return PLATFORM_UNKNOWN;
#endif
}

int SCB_PlatformMatches(SCB_Platform current, const char *target) {
	if (!target || strcmp(target, "default") == 0) return 1;
	if (strcmp(target, "linux") == 0)   return current == PLATFORM_LINUX;
	if (strcmp(target, "macos") == 0)   return current == PLATFORM_MACOS;
	if (strcmp(target, "windows") == 0) return current == PLATFORM_WINDOWS;
	if (strcmp(target, "unix") == 0)    return current == PLATFORM_LINUX || current == PLATFORM_MACOS;
	return 0;
}

void SCB_EnsureBuildDir(void)
{
	struct stat st = {0};
	if (stat("build", &st) == -1)
	{
		mkdir("build", 0700);
	}
}


int SCB_NeedsRebuild(const char *source, const char *object, const char *outputExecutable) {
	struct stat st_source, st_object, st_exec;

	// If source doesn't exist: error
	if (stat(source, &st_source) != 0)
		return 1;

	// If object doesn't exist: must rebuild
	if (stat(object, &st_object) != 0)
		return 1;

	// If executable doesn't exist: must rebuild
	if (stat(outputExecutable, &st_exec) != 0)
		return 1;

	// If source or object is newer than executable: rebuild
	if (st_source.st_mtime > st_exec.st_mtime) return 1;
	if (st_object.st_mtime > st_exec.st_mtime) return 1;

	// Otherwise, skip
	return 0;
}

char *StrDupTrim(const char *start, size_t len) {
	while (len && isspace(*start)) { ++start; --len; }
	while (len && isspace(start[len - 1])) { --len; }
	char *s = malloc(len + 1);
	if (!s) return NULL;
	memcpy(s, start, len);
	s[len] = '\0';
	return s;
}

void SCB_ParseDirective(char *line, char **directiveOut, char **valueOut, char **platformOut) {
	char *at = strchr(line, '@');
	if (!at) return;

	char *start = strchr(at, '(');
	char *end = strrchr(at, ')');
	if (!start || !end || end <= start) return;

	*directiveOut = StrDupTrim(at + 1, start - (at + 1));
	char *content = StrDupTrim(start + 1, end - start - 1);

	char *comma = strchr(content, ',');
	if (comma) {
		*comma = '\0';
		*valueOut = StrDupTrim(content, strlen(content));
		char *platformPart = strstr(comma + 1, "platform=");
		if (platformPart) {
			*platformOut = strdup(platformPart + strlen("platform="));
			for (int i = 0; (*platformOut)[i]; ++i) {
				if (isspace((*platformOut)[i]) || (*platformOut)[i] == ')')
					(*platformOut)[i] = '\0';
			}
		}
	} else {
		*valueOut = strdup(content);
	}
	free(content);
}

SCB_FileConfig *SCB_GetFileConfig(char *filepath) {
	FILE *file = fopen(filepath, "r");
	if (!file) {
		fprintf(stderr, "[ERROR]: failed to open %s\n", filepath);
		exit(1);
	}

	SCB_FileConfig *config = calloc(1, sizeof(SCB_FileConfig));
	config->filepath = strdup(filepath);
	SCB_Platform currentPlatform = SCB_GetCurrentPlatform();

	char line[1024];
	while (fgets(line, sizeof(line), file))
	{
		if (!strstr(line, "// SCB:"))
		{
			continue;
		}

		char *directive = NULL, *value = NULL, *platform = NULL;
		SCB_ParseDirective(line, &directive, &value, &platform);

		if (!directive)
		{
			continue;
		}
		if (SCB_PlatformMatches(currentPlatform, platform)) {
			if (strcmp(directive, "cc") == 0)         config->cc = strdup(value);
			else if (strcmp(directive, "cflags") == 0) config->cflags = strdup(value);
			else if (strcmp(directive, "ldflags") == 0) config->ldflags = strdup(value);
		}

		free(directive); free(value); free(platform);
	}
	fclose(file);
	return config;
}

int SCB_InitGlobalConfig(const char *mainFile) {
	FILE *file = fopen(mainFile, "r");
	if (!file) {
		fprintf(stderr, "[ERROR]: Failed to open %s\n", mainFile);
		exit(1);
	}

	GlobalConfig.sourcePaths = realloc(GlobalConfig.sourcePaths, sizeof(char *) * (GlobalConfig.sourceCount + 1));
	GlobalConfig.sourcePaths[GlobalConfig.sourceCount++] = strdup(mainFile);
	SCB_Platform current = SCB_GetCurrentPlatform();

	char line[1024];
	while (fgets(line, sizeof(line), file)) {
		if (!strstr(line, "// SCB:")) continue;

		char *directive = NULL, *value = NULL, *platform = NULL;
		SCB_ParseDirective(line, &directive, &value, &platform);

		if (!directive)
		{
			continue;
		}
		if (SCB_PlatformMatches(current, platform))
		{
			if (strcmp(directive, "output") == 0)
			{
				GlobalConfig.output = strdup(value);
			}
			else if (strcmp(directive, "sources") == 0)
			{
				char *token = strtok(value, " ");
				while (token)
				{
					GlobalConfig.sourcePaths = realloc(GlobalConfig.sourcePaths, sizeof(char *) * (GlobalConfig.sourceCount + 1));
					GlobalConfig.sourcePaths[GlobalConfig.sourceCount++] = strdup(token);
					token = strtok(NULL, " ");
				}
			}
			else if (strcmp(directive, "global-cc") == 0)
			{
				GlobalConfig.cc = strdup(value);
			}
			else if (strcmp(directive, "ld") == 0)
			{
				GlobalConfig.ld = strdup(value);
			}
			else if (strcmp(directive, "global-cflags") == 0)
			{
				GlobalConfig.cflags = strdup(value);
			}
			else if (strcmp(directive, "global-ldflags") == 0)
			{
				GlobalConfig.ldflags = strdup(value);
			}
		}

		free(directive); free(value); free(platform);
	}

	if (!GlobalConfig.cc) GlobalConfig.cc = strdup(DEFAULT_CC);
	if (!GlobalConfig.ld) GlobalConfig.ld = strdup(DEFAULT_LD);
	fclose(file);
	return 0;
}


int SCB_ExecuteFileConfig(SCB_FileConfig *cfg) {
	if (!cfg || !cfg->filepath) return -1;
	const char *cc = cfg->cc ? cfg->cc : GlobalConfig.cc;
	if (!cc) return -1;

	SCB_EnsureBuildDir();

	char *filename = strrchr(cfg->filepath, '/');
	filename = filename ? filename + 1 : cfg->filepath;

	char outputPath[512];
	snprintf(outputPath, sizeof(outputPath), "build/%s.o", filename);

	char cmd[1024] = {0};
	snprintf(cmd, sizeof(cmd), "%s ", cc);
	if (GlobalConfig.cflags)
	{
		strncat(cmd, GlobalConfig.cflags, sizeof(cmd) - strlen(cmd) - 1);
		strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
	}
	if (cfg->cflags)
	{
		strncat(cmd, cfg->cflags, sizeof(cmd) - strlen(cmd) - 1);
		strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
	}
	strncat(cmd, "-c ", sizeof(cmd) - strlen(cmd) - 1);
	strncat(cmd, cfg->filepath, sizeof(cmd) - strlen(cmd) - 1);
	strncat(cmd, " -o ", sizeof(cmd) - strlen(cmd) - 1);
	strncat(cmd, outputPath, sizeof(cmd) - strlen(cmd) - 1);
	if (!SCB_NeedsRebuild(cfg->filepath, outputPath, GlobalConfig.output))
	{
		printf("[SCB] Skipping: %s (up-to-date)\n", cfg->filepath);
		return (0);
	}
	printf("[SCB] Compiling: %s\n", cmd);
	return system(cmd);
}



int SCB_LinkExecutable(void)
{
	if (!GlobalConfig.output)
	{
		fprintf(stderr, "[ERROR]: no output specified\n");
		return -1;
	}

	char cmd[2048] = {0};
	snprintf(cmd, sizeof(cmd), "%s ", GlobalConfig.ld);

	for (int i = 0; i < GlobalConfig.sourceCount; ++i)
	{
		char *filename = strrchr(GlobalConfig.sources[i]->filepath, '/');
		filename = filename ? filename + 1 : GlobalConfig.sources[i]->filepath;

		strcat(cmd, "build/");
		strcat(cmd, filename);
		strcat(cmd, ".o ");
	}

	strcat(cmd, "-o ");
	strcat(cmd, GlobalConfig.output);

	for (int j = 0; j < GlobalConfig.sourceCount; ++j)
	{
		SCB_FileConfig *cfg = GlobalConfig.sources[j];
		if (cfg && cfg->ldflags)
		{
			strcat(cmd, " ");
			strcat(cmd, cfg->ldflags);
		}
	}

	printf("[SCB] Linking: %s\n", cmd);
	return system(cmd);
}


int SCB_ExecuteCommand(char *cmd)
{
	printf("[SCB]: Executing command : %s\n", cmd);
	return GlobalConfig.dryRun ? 0 : system(cmd);
}

int SCB_Load(const char *mainFile)
{
	SCB_InitGlobalConfig(mainFile);
	GlobalConfig.sources = malloc(sizeof(SCB_FileConfig *) * GlobalConfig.sourceCount);
	for (int i = 0; i < GlobalConfig.sourceCount; i++)
	{
		GlobalConfig.sources[i] = SCB_GetFileConfig(GlobalConfig.sourcePaths[i]);
		SCB_ExecuteFileConfig(GlobalConfig.sources[i]);
	}
	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <main.c>\n", argv[0]);
		return 1;
	}

	SCB_Load(argv[1]);
	SCB_LinkExecutable();

	// char run[512];
	// snprintf(run, sizeof(run), "./%s", GlobalConfig.output);
	// return SCB_ExecuteCommand(run);
}

