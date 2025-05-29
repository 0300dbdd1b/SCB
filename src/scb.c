// SCB: @global-cc(gcc)
// SCB: @ld(gcc)
// SCB: @cflags(-pedantic-errors)
// SCB: @output(scb)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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

typedef struct SCB_FileConfig
{
	char *filepath;
	char *cc;
	char *cflags;
	char *ldflags;
} SCB_FileConfig;

typedef struct SCB_GlobalConfig
{
	char *output;
	char **sourcePaths;
	SCB_FileConfig	**sources;
	int sourceCount;
	char *cc;
	char *ld;
	char *cflags;
	char *ldflags;
} SCB_GlobalConfig;

typedef enum SCB_Platform
{
	PLATFORM_LINUX,
	PLATFORM_MACOS,
	PLATFORM_WINDOWS,
} SCB_Platform;

SCB_Platform SCB_GetCurrentPlatform()
{
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

SCB_GlobalConfig GlobalConfig = {0};


int SCB_PlatformMatches(SCB_Platform current, const char *target)
{
	if (!target) return 1;
	if (strcmp(target, "default") == 0)	return 1;
	if (strcmp(target, "linux") == 0)	return current == PLATFORM_LINUX;
	if (strcmp(target, "macos") == 0)	return current == PLATFORM_MACOS;
	if (strcmp(target, "windows") == 0)	return current == PLATFORM_WINDOWS;
	if (strcmp(target, "unix") == 0)	return current == PLATFORM_LINUX || current == PLATFORM_MACOS;
	return 0;
}



char *StrDupTrim(const char *start, size_t len)
{
	while (len && isspace(*start))
	{
		++start;
		--len;
	}
	while (len && isspace(start[len - 1]))
	{
		--len;
	}
	char *s = malloc(len + 1);
	if (!s) return NULL;
	memcpy(s, start, len);
	s[len] = '\0';
	return s;
}

SCB_FileConfig	*SCB_GetFileConfig(char *filepath)
{
	SCB_FileConfig *config = malloc(sizeof(SCB_FileConfig));
	memset(config, 0, sizeof(SCB_FileConfig));
	FILE *file = fopen(filepath, "r");
	if (!file)
	{
		fprintf(stderr, "[ERROR]: failed to open %s", filepath);
		exit(1);
	}
	SCB_Platform currentPlatform = SCB_GetCurrentPlatform();
	config->filepath = strdup(filepath);
	char line[1024];
	while (fgets(line, sizeof(line), file))
	{
		char *jitStart = strstr(line, "// SCB:");
		if (!jitStart) continue;
		char *at = strchr(jitStart, '@');
		if (!at) continue;
		char *parenStart = strchr(at, '('); char *parenEnd = strrchr(at, ')');
		if (!parenStart || !parenEnd || parenEnd < parenStart) continue;
		size_t directiveLen = parenStart - (at + 1);
		char *directive = StrDupTrim(at + 1, directiveLen);

		char *inside = StrDupTrim(parenStart + 1, parenEnd - parenStart - 1);
		char *comma = strchr(inside, ',');
		char *value = NULL;
		char *platform = NULL;

		if (comma)
		{
			*comma = '\0';
			value = StrDupTrim(inside, strlen(inside));
			char *platformPart = strstr(comma + 1, "platform=");
			if (platformPart)
			{
				platform = strdup(platformPart + strlen("platform="));
				for (int i = 0; platform[i]; ++i)
				{
					if (isspace(platform[i]) || platform[i] == ')')
		 			{
						platform[i] = '\0';
					}
				}
			}
		}
		else
		{
			value = strdup(inside);
		}

		if(SCB_PlatformMatches(currentPlatform, platform))
		{
			if (strcmp(directive, "cc") == 0)
			{
				config->cc = strdup(value);
			}
			else if (strcmp(directive, "cflags") == 0)
			{
				config->cflags = strdup(value);
			}
			else if (strcmp(directive, "ldflags") == 0)
			{
				config->ldflags = strdup(value);
			}
		}
		free(directive);
		free(inside);
		free(value);
		free(platform);
	}
	fclose(file);
	return (config);
}


int SCB_InitGlobalConfig(const char *mainFile)
{
	FILE *file = fopen(mainFile, "r");
	if (!file)
	{
		fprintf(stderr, "[ERROR]: Failed to open %s\n", mainFile);
		exit(1);
	}

	char line[1024];
	SCB_Platform current = SCB_GetCurrentPlatform();
	GlobalConfig.sourcePaths = realloc(GlobalConfig.sourcePaths, sizeof(char *) * (GlobalConfig.sourceCount + 1));
	GlobalConfig.sourcePaths[GlobalConfig.sourceCount++] = strdup(mainFile);
	while (fgets(line, sizeof(line), file))
	{
		char *scb = strstr(line, "// SCB:");
		if (!scb) continue;
		char *at = strchr(scb, '@');
		if (!at) continue;
		char *start = strchr(at, '(');
		char *end = strrchr(at, ')');
		if (!start || !end || end < start) continue;

		char *directive = StrDupTrim(at + 1, start - (at + 1));
		char *content = StrDupTrim(start + 1, end - start - 1);

		char *comma = strchr(content, ',');
		char *value = NULL;
		char *platform = NULL;

		if (comma)
		{
			*comma = '\0';
			value = StrDupTrim(content, strlen(content));
			char *platformPart = strstr(comma + 1, "platform=");
			if (platformPart)
			{
				platform = strdup(platformPart + strlen("platform="));
			}
		}
		else
		{
			value = strdup(content);
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
		free(directive);
		free(content);
		free(value);
		free(platform);
	}

	fclose(file);
	return 0;
}




int	SCB_ExecuteFileConfig(SCB_FileConfig *config)
{
	if (!config || !config->filepath)
	{
		fprintf(stderr, "[ERROR]: Missing filepath in file config\n");
		return -1;
	}

	const char *cc = config->cc ? config->cc : GlobalConfig.cc;
	if (!cc)
	{
		fprintf(stderr, "[ERROR]: No compiler specified for %s\n", config->filepath);
		return -1;
	}

	char command[1024] = {0};
	snprintf(command, sizeof(command), "%s ", cc);

	if (GlobalConfig.cflags)
	{
		strncat(command, GlobalConfig.cflags, sizeof(command) - strlen(command) - 1);
		strncat(command, " ", sizeof(command) - strlen(command) - 1);
	}
	if (config->cflags) {
		strncat(command, config->cflags, sizeof(command) - strlen(command) - 1);
		strncat(command, " ", sizeof(command) - strlen(command) - 1);
	}

	strncat(command, "-c ", sizeof(command) - strlen(command) - 1);
	strncat(command, config->filepath, sizeof(command) - strlen(command) - 1);

	strncat(command, " -o ", sizeof(command) - strlen(command) - 1);
	strncat(command, config->filepath, sizeof(command) - strlen(command) - 1);
	strncat(command, ".o", sizeof(command) - strlen(command) - 1);

	printf("[SCB] Compiling: %s\n", command);
	return system(command);
}

int SCB_Load(const char *mainFile)
{
	SCB_InitGlobalConfig(mainFile);
	GlobalConfig.sources = malloc(sizeof(SCB_FileConfig *) * GlobalConfig.sourceCount);

	for (int i = 0; i < GlobalConfig.sourceCount; i++)
	{
		GlobalConfig.sources[i] = SCB_GetFileConfig(GlobalConfig.sourcePaths[i]);
		if (!GlobalConfig.sources[i])
		{
			fprintf(stderr, "[WARNING]: Failed to get file config for %s\n", GlobalConfig.sourcePaths[i]);
			continue;
		}
		SCB_ExecuteFileConfig(GlobalConfig.sources[i]);
	}
	return 0;
}

void SCB_PrintFileConfig(SCB_FileConfig *config)
{
	printf("filepath : %s\n", config->filepath ? config->filepath : "(null)");
	printf("cc : %s\n", config->cc);
	printf("cflags : %s\n", config->cflags);
	printf("ldflags : %s\n", config->ldflags);

}

void SCB_PrintConfig(void)
{
	printf("output: %s\n", GlobalConfig.output);
	printf("global cflags: %s\n", GlobalConfig.cflags);
	printf("global ldflags: %s\n", GlobalConfig.ldflags);
	printf("source count: %d\n", GlobalConfig.sourceCount);

	for (int i = 0; i < GlobalConfig.sourceCount; i++)
	{
		SCB_PrintFileConfig(GlobalConfig.sources[i]);
	}
}

int SCB_LinkExecutable()
{
	if (!GlobalConfig.output)
	{
		fprintf(stderr, "[ERROR]: no output specified\n");
		return -1;
	}

	char command[2048] = {0};
	snprintf(command, sizeof(command), "%s ", GlobalConfig.ld);
	for (int i = 0; i < GlobalConfig.sourceCount; ++i)
	{
		strcat(command, GlobalConfig.sources[i]->filepath);
		strcat(command, ".o ");
	}

	strcat(command, "-o ");
	strcat(command, GlobalConfig.output);


	for (int j = 0; j < GlobalConfig.sourceCount; ++j)
	{
		SCB_FileConfig *cfg = GlobalConfig.sources[j];
		if (!cfg)
		{
			fprintf(stderr, "[WARNING]: NULL source config at index %d\n", j);
			continue;
		}
		if (cfg->ldflags)
		{
			strcat(command, " ");
			strcat(command, cfg->ldflags);
		}
	}

	printf("[SCB] Linking: %s\n", command);
	return system(command);
}

// TODO: [_] add program arguments.
// TODO: [_] move .o files to build/ folder
// TODO: [_] better path management (absoulte?)
// TODO: [_] handle * for sourcefiles (eg: src/*.c)
// TODO: [_] timestamp checks before rebuilding
int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: ./scb <main.c>\n");
		return 1;
	}

	SCB_Load(argv[1]);
	/* SCB_PrintConfig(); */
	SCB_LinkExecutable();

	char run[512];
	snprintf(run, sizeof(run), "./%s", GlobalConfig.output);
	system(run);
	return 0;
}

