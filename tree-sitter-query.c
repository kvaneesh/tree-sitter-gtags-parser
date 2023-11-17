/*
 * Copyright (c) 2023
 *	Aneesh Kumar K.V <aneesh.kumar@kernel.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <tree_sitter/api.h>
#include <assert.h>
#include <dlfcn.h>
#include <limits.h>
#include <sys/stat.h>

#include "parser.h"

//#define DEBUG

#ifdef DEBUG
#define prdebug(fmt, ...)			\
	printf(fmt, ##__VA_ARGS__)
#else
#define prdebug(fmt, ...)
#endif

#define READ_CHUNK_SIZE 500
#define MAX_TOKEN_SIZE 500
#define MAX_LINE_SIZE  200
TSLanguage * (*tree_sitter_language)(void);

struct Buf_Read {
	char *buffer;
	int fd;
};

static const char *read_file(void *payload, uint32_t byte_index,
			     TSPoint position, uint32_t *bytes_read)
{
	int ret;
	int fd = ((struct Buf_Read *)payload)->fd;
	char *buf =  ((struct Buf_Read *)payload)->buffer;

	ret = pread(fd, buf, 10, byte_index);
	*bytes_read = ret;
	return (char *) buf;
}

static void fetch_node_details(int fd, TSNode node, char *details)
{
	int size;
	char *new_line;
	int start_byte, end_byte;

	start_byte = ts_node_start_byte(node);
	end_byte = ts_node_end_byte(node);
	size = end_byte - start_byte;

	if (size >= MAX_TOKEN_SIZE)
		size = MAX_TOKEN_SIZE - 1;
	pread(fd, details, size, start_byte);
	details[size] = '\0';

	/*
	 * gtags database sorting seems to depend on this in weird ways
	 * If there is new line in the read line, convert that to NULL so we don't read beyond
	 */
	new_line = strchr(details, '\n');
	if (new_line)
		*new_line = '\0';
}

static char *guess_language_from_filename(const char *file)
{
	if (strstr(file, ".c") || strstr(file, ".h"))
		return "c";
	return NULL;
}

static char *fetch_query_string(char *lang, char *type)
{
	int fd;
	char *query_string;
	struct stat st_buf;
	char path_name[PATH_MAX];
	char *query_path = getenv("GTAGS_TREE_SITTER_QUERY_PATH");

	if (!query_path)
		return NULL;

	sprintf(path_name, "%s/%s/%s.scm", query_path, lang, type);
	fd = open(path_name, O_RDONLY);
	if (fd == -1)
		return NULL;

	if (fstat(fd, &st_buf) == -1)
		return NULL;

	query_string = calloc(st_buf.st_size, 1);

	read(fd, query_string, st_buf.st_size);
	close(fd);

	return query_string;
}

static char *fetch_tags_query(char *lang)
{
	return fetch_query_string(lang, "tags");
}

static char *fetch_reference_query(char *lang)
{
	return fetch_query_string(lang, "ref");
}

static TSQueryCursor *alloc_cached_cursor()
{
	static TSQueryCursor *cursor;
	if (!cursor)
		cursor = ts_query_cursor_new();
	return cursor;
}

static void tree_sitter_parse(const struct parser_param *param, int type)
{
	struct Buf_Read buf;
	char *query_string;
	char read_buffer[READ_CHUNK_SIZE];

	TSParser *parser = ts_parser_new();
	ts_parser_set_language(parser, (*tree_sitter_language)());

	/* Copy the file into BUFFER. */
	buf.fd = open(param->file, O_RDONLY);
	if (buf.fd == -1)
		param->die("Cannot open file %s", param->file);
	buf.buffer = &read_buffer[0];

	TSInput input = {&buf, read_file, TSInputEncodingUTF8};
	TSTree *tree = ts_parser_parse(parser, NULL, input);

	// Get the root node of the syntax tree.
	TSNode root_node = ts_tree_root_node(tree);
	if (type == PARSER_DEF)
		query_string = fetch_tags_query(guess_language_from_filename(param->file));
	else
		query_string = fetch_reference_query(guess_language_from_filename(param->file));

	if (!query_string)
		param->die("Failed to fetch query from query directory\n");

	uint32_t error_offset = 0;
	TSQueryError error_type = TSQueryErrorNone;

	TSQuery *query = ts_query_new((*tree_sitter_language)(), query_string,
				      strlen(query_string), &error_offset, &error_type);
	if (!query) {
		param->warning("Failed to create a query from query string %s\n", query_string);
		goto error_query;
	}

	// Execute the query on the syntax tree
	TSQueryCursor *cursor = alloc_cached_cursor();
	if (!cursor)
		param->die("Failed to allocate cursor\n");

	ts_query_cursor_exec(cursor, query, root_node);

	TSQueryMatch match;
	// Iterate over the query matches and print the function names
	while (ts_query_cursor_next_match(cursor, &match)) {

		TSPoint point;
		char name[MAX_TOKEN_SIZE] = {0};
		char definition[MAX_TOKEN_SIZE] = {0};

		if (match.capture_count != 2) {
			param->warning("Match don't have two objects (query string %s)\n", query_string);
			goto error_match;
		}
		fetch_node_details(buf.fd, match.captures[1].node, name);
		fetch_node_details(buf.fd, match.captures[0].node, definition);

		point = ts_node_start_point(match.captures[0].node);
		prdebug("type = %d tag = :%s: lineno: %d file %s line :%s:\n", type, name, point.row + 1, param->file, definition);
		param->put(type, name, point.row + 1, param->file, definition, param->arg);
	}

error_match:
	// Clean up resources
	// cursor details are cached
	//ts_query_cursor_delete(cursor);
	ts_query_delete(query);

error_query:
	ts_tree_delete(tree);
	ts_parser_delete(parser);
	close(buf.fd);
	free(query_string);
}

static void tree_sitter_parse_tags(const struct parser_param *param)
{
	return tree_sitter_parse(param, PARSER_DEF);
}

static void tree_sitter_parse_references(const struct parser_param *param)
{
	return tree_sitter_parse(param, PARSER_REF_SYM);
}

void parser(const struct parser_param *param)
{
	void *handle;
	char *language, lib[PATH_MAX], function[MAX_TOKEN_SIZE];
	char *language_path = getenv("GTAGS_TREE_SITTER_LANG_PATH");

	assert(param->size >= sizeof(*param));

	if (!language_path)
		param->die("lib path for tree-sitter grammer files not exported (GTAGS_TREE_SITTER_LANG_PATH)");

	language = guess_language_from_filename(param->file);
	if (!language) {
		param->warning("Language not identified for file %s", param->file);
		return;
	}

	sprintf(lib, "%s/libtree-sitter-%s.so", language_path, language);
	sprintf(function, "tree_sitter_%s", language);

	handle = dlopen(lib, RTLD_LAZY);
	if (!handle)
		param->die("Cannot load lib %s", lib);

	tree_sitter_language = dlsym(handle, function);
	if (!tree_sitter_language)
		param->die("Cannot lookup function %s in lib %s", function, lib);

	tree_sitter_parse_tags(param);
	tree_sitter_parse_references(param);
	dlclose(handle);
}
