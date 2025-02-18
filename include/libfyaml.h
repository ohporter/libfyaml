/*
 * libfyaml.h - Main header file of the public interface
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBFYAML_H
#define LIBFYAML_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

/* opaque types for the user */
struct fy_token;
struct fy_document_state;
struct fy_parser;
struct fy_emitter;
struct fy_document;
struct fy_node;
struct fy_node_pair;
struct fy_anchor;
struct fy_node_mapping_sort_ctx;

#ifndef FY_BIT
#define FY_BIT(x) (1U << (x))
#endif

/**
 * DOC: libfyaml public API
 *
 */

/**
 * struct fy_version - The YAML version
 *
 * @major: Major version number
 * @minor: Major version number
 *
 * The parser fills it according to the \%YAML directive
 * found in the document.
 */
struct fy_version {
	int major;
	int minor;
};

/**
 * struct fy_tag - The YAML tag structure.
 *
 * @handle: Handle of the tag (i.e. `"!!"` )
 * @prefix: The prefix of the tag (i.e. `"tag:yaml.org,2002:"`
 *
 * The parser fills it according to the \%TAG directives
 * encountered during parsing.
 */
struct fy_tag {
	const char *handle;
	const char *prefix;
};

/**
 * struct fy_mark - marker holding information about a location
 *		    in a &struct fy_input
 *
 * @input_pos: Position of the mark (from the start of the current input)
 * @line: Line position (0 index based)
 * @column: Column position (0 index based)
 */
struct fy_mark {
	size_t input_pos;
	int line;
	int column;
};

/**
 * enum fy_error_type - The supported diagnostic/error types
 *
 * @FYET_DEBUG: Debug level (disabled if library is not compiled in debug mode)
 * @FYET_INFO: Informational level
 * @FYET_NOTICE: Notice level
 * @FYET_WARNING: Warning level
 * @FYET_ERROR: Error level - error reporting is using this level
 * @FYET_MAX: Non inclusive maximum fy_error_type value
 *
 */
enum fy_error_type {
	FYET_DEBUG,
	FYET_INFO,
	FYET_NOTICE,
	FYET_WARNING,
	FYET_ERROR,
	FYET_MAX,
};

/**
 * enum fy_error_module - Module which generated the diagnostic/error
 *
 * @FYEM_UNKNOWN: Unknown, default if not specific
 * @FYEM_ATOM: Atom module, used by atom chunking
 * @FYEM_SCAN: Scanner module
 * @FYEM_PARSE: Parser module
 * @FYEM_DOC: Document module
 * @FYEM_BUILD: Build document module (after tree is constructed)
 * @FYEM_INTERNAL: Internal error/diagnostic module
 * @FYEM_SYSTEM: System error/diagnostic module
 * @FYEM_MAX: Non inclusive maximum fy_error_module value
 */
enum fy_error_module {
	FYEM_UNKNOWN,
	FYEM_ATOM,
	FYEM_SCAN,
	FYEM_PARSE,
	FYEM_DOC,
	FYEM_BUILD,
	FYEM_INTERNAL,
	FYEM_SYSTEM,
	FYEM_MAX,
};

/* Shift amount to apply for color option */
#define FYPCF_COLOR_SHIFT		2
/* Mask of bits of the color option */
#define FYPCF_COLOR_MASK		3
/* Build a color option */
#define FYPCF_COLOR(x)			(((unsigned int)(x) & FYPCF_COLOR_MASK) << FYPCF_COLOR_SHIFT)
/* Shift amount to apply to the module mask options */
#define FYPCF_MODULE_SHIFT		4
/* Mask of bits of the module options */
#define FYPCF_MODULE_MASK		((1U << 8) - 1)
/* Shift amount of the debug level option */
#define FYPCF_DEBUG_LEVEL_SHIFT		12
/* Mask of bits of the debug level option */
#define FYPCF_DEBUG_LEVEL_MASK		((1U << 4) - 1)
/* Build a debug level option */
#define FYPCF_DEBUG_LEVEL(x)		(((unsigned int)(x) & FYPCF_DEBUG_LEVEL_MASK) << FYPCF_DEBUG_LEVEL_SHIFT)
/* Shift amount of the debug diagnostric output options */
#define FYPCF_DEBUG_DIAG_SHIFT		16
/* Mask of the debug diagnostric output options */
#define FYPCF_DEBUG_DIAG_MASK		((1U << 4) - 1)

/**
 * enum fy_parse_cfg_flags - Parse configuration flags
 *
 * These flags control the operation of the parse and the debugging
 * output/error reporting via filling in the &fy_parse_cfg->flags member.
 *
 * @FYPCF_QUIET: Quiet, do not output any information messages
 * @FYPCF_COLLECT_DIAG: Collect diagnostic/error messages
 * @FYPCF_COLOR_AUTO: Automatically use color, i.e. the diagnostic output is a tty
 * @FYPCF_COLOR_NONE: Never use color for diagnostic output
 * @FYPCF_COLOR_FORCE: Force use of color for diagnostic output
 * @FYPCF_DEBUG_UNKNOWN: Enable diagnostic output by the unknown module
 * @FYPCF_DEBUG_ATOM: Enable diagnostic output by the atom module
 * @FYPCF_DEBUG_SCAN: Enable diagnostic output by the scan module
 * @FYPCF_DEBUG_PARSE: Enable diagnostic output by the parse module
 * @FYPCF_DEBUG_DOC: Enable diagnostic output by the document module
 * @FYPCF_DEBUG_BUILD: Enable diagnostic output by the build document module
 * @FYPCF_DEBUG_INTERNAL: Enable diagnostic output by the internal module
 * @FYPCF_DEBUG_SYSTEM: Enable diagnostic output by the system module
 * @FYPCF_DEBUG_LEVEL_DEBUG: Set the debug level to %FYET_DEBUG
 * @FYPCF_DEBUG_LEVEL_INFO: Set the debug level to %FYET_INFO
 * @FYPCF_DEBUG_LEVEL_NOTICE: Set the debug level to %FYET_NOTICE
 * @FYPCF_DEBUG_LEVEL_WARNING: Set the debug level to %FYET_WARNING
 * @FYPCF_DEBUG_LEVEL_ERROR: Set the debug level to %FYET_ERROR
 * @FYPCF_DEBUG_DIAG_SOURCE: Include source location in the diagnostic output
 * @FYPCF_DEBUG_DIAG_POSITION: Include source file location in the diagnostic output
 * @FYPCF_DEBUG_DIAG_TYPE: Include the debug type in the diagnostic output
 * @FYPCF_DEBUG_DIAG_MODULE: Include the debug module in the diagnostic output
 * @FYPCF_RESOLVE_DOCUMENT: When producing documents, automatically resolve them
 * @FYPCF_DISABLE_MMAP_OPT: Disable mmap optimization
 * @FYPCF_DISABLE_RECYCLING: Disable recycling optimization
 */
enum fy_parse_cfg_flags {
	FYPCF_QUIET			= FY_BIT(0),
	FYPCF_COLLECT_DIAG		= FY_BIT(1),
	FYPCF_COLOR_AUTO		= FYPCF_COLOR(0),
	FYPCF_COLOR_NONE		= FYPCF_COLOR(1),
	FYPCF_COLOR_FORCE		= FYPCF_COLOR(2),
	FYPCF_DEBUG_UNKNOWN		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_UNKNOWN),
	FYPCF_DEBUG_ATOM		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_ATOM),
	FYPCF_DEBUG_SCAN		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_SCAN),
	FYPCF_DEBUG_PARSE		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_PARSE),
	FYPCF_DEBUG_DOC			= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_DOC),
	FYPCF_DEBUG_BUILD		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_BUILD),
	FYPCF_DEBUG_INTERNAL		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_INTERNAL),
	FYPCF_DEBUG_SYSTEM		= FY_BIT(FYPCF_MODULE_SHIFT + FYEM_SYSTEM),
	FYPCF_DEBUG_LEVEL_DEBUG		= FYPCF_DEBUG_LEVEL(FYET_DEBUG),
	FYPCF_DEBUG_LEVEL_INFO		= FYPCF_DEBUG_LEVEL(FYET_INFO),
	FYPCF_DEBUG_LEVEL_NOTICE	= FYPCF_DEBUG_LEVEL(FYET_NOTICE),
	FYPCF_DEBUG_LEVEL_WARNING	= FYPCF_DEBUG_LEVEL(FYET_WARNING),
	FYPCF_DEBUG_LEVEL_ERROR		= FYPCF_DEBUG_LEVEL(FYET_ERROR),
	FYPCF_DEBUG_DIAG_SOURCE		= FY_BIT(FYPCF_DEBUG_DIAG_SHIFT + 0),
	FYPCF_DEBUG_DIAG_POSITION	= FY_BIT(FYPCF_DEBUG_DIAG_SHIFT + 1),
	FYPCF_DEBUG_DIAG_TYPE		= FY_BIT(FYPCF_DEBUG_DIAG_SHIFT + 2),
	FYPCF_DEBUG_DIAG_MODULE		= FY_BIT(FYPCF_DEBUG_DIAG_SHIFT + 3),
	FYPCF_RESOLVE_DOCUMENT		= FY_BIT(20),
	FYPCF_DISABLE_MMAP_OPT		= FY_BIT(21),
	FYPCF_DISABLE_RECYCLING		= FY_BIT(22)
};

/* Enable diagnostic output by all modules */
#define FYPCF_DEBUG_ALL			(FYPCF_MODULE_MASK << FYPCF_MODULE_SHIFT)
/* Sane default for enabling diagnostic output */
#define FYPCF_DEBUG_DEFAULT 		(FYPCF_DEBUG_ALL & ~FYPCF_DEBUG_ATOM)

/* Include every meta diagnostic output */
#define FYPCF_DEBUG_DIAG_ALL		(FYPCF_DEBUG_DIAG_MASK << FYPCF_DEBUG_DIAG_SHIFT)
/* Sane default of the meta diagnostic output */
#define FYPCF_DEBUG_DIAG_DEFAULT	(FYPCF_DEBUG_DIAG_TYPE)

#define FYPCF_GET_DEBUG_LEVEL(_f) \
	(((unsigned int)(_f) >> FYPCF_DEBUG_LEVEL_SHIFT) & FYPCF_DEBUG_LEVEL_MASK)

/* for debugging without a parser context */
void fy_set_default_parser_cfg_flags(enum fy_parse_cfg_flags pflags);

/**
 * struct fy_parse_cfg - parser configuration structure.
 *
 * Argument to the fy_parser_create() method which
 * perform parsing of YAML files.
 *
 * @search_path: Search path when accessing files, seperate with ':'
 * @flags: Configuration flags
 * @userdata: Opaque user data pointer
 */
struct fy_parse_cfg {
	const char *search_path;
	enum fy_parse_cfg_flags flags;
	void *userdata;
};

/**
 * enum fy_event_type - Event types
 *
 * @FYET_NONE: No event
 * @FYET_STREAM_START: Stream start event
 * @FYET_STREAM_END: Stream end event
 * @FYET_DOCUMENT_START: Document start event
 * @FYET_DOCUMENT_END: Document end event
 * @FYET_MAPPING_START: YAML mapping start event
 * @FYET_MAPPING_END: YAML mapping end event
 * @FYET_SEQUENCE_START: YAML sequence start event
 * @FYET_SEQUENCE_END: YAML sequence end event
 * @FYET_SCALAR: YAML scalar event
 * @FYET_ALIAS: YAML alias event
 */
enum fy_event_type {
	FYET_NONE,
	FYET_STREAM_START,
	FYET_STREAM_END,
	FYET_DOCUMENT_START,
	FYET_DOCUMENT_END,
	FYET_MAPPING_START,
	FYET_MAPPING_END,
	FYET_SEQUENCE_START,
	FYET_SEQUENCE_END,
	FYET_SCALAR,
	FYET_ALIAS,
};

/**
 * enum fy_scalar_style - Scalar styles supported by the parser/emitter
 *
 * @FYSS_ANY: Any scalar style, not generated by the parser.
 * 	      Lets the emitter to choose
 * @FYSS_PLAIN: Plain scalar style
 * @FYSS_SINGLE_QUOTED: Single quoted style
 * @FYSS_DOUBLE_QUOTED: Double quoted style
 * @FYSS_LITERAL: YAML literal block style
 * @FYSS_FOLDED: YAML folded block style
 * @FYSS_MAX: marks end of scalar styles
 */
enum fy_scalar_style {
	FYSS_ANY = -1,
	FYSS_PLAIN,
	FYSS_SINGLE_QUOTED,
	FYSS_DOUBLE_QUOTED,
	FYSS_LITERAL,
	FYSS_FOLDED,
	FYSS_MAX,
};

/**
 * struct fy_event - Event generated by the parser
 *
 * This structure is generated by the parser by each call
 * to fy_parser_parse() and release by fy_parser_event_free()
 *
 * @type: Type of the event, see &enum fy_event_type
 *
 * @stream_start: Stream start information, it is valid when
 *                &fy_event->type is &enum FYET_STREAM_START
 * @stream_start.stream_start: The token that started the stream
 * @stream_end:Stream end information, it is valid when
 *             &fy_event->type is &enum FYET_STREAM_END
 * @stream_end.stream_end: The token that ended the stream
 *
 * @document_start: Document start information, it is valid when
 *                  &fy_event->type is &enum FYET_DOCUMENT_START
 * @document_start.document_start: The token that started the document, or
 *                                 NULL if the document was implicitly
 *                                 started
 * @document_start.document_state: The state of the document (i.e. information
 *                                 about the YAML version and configured tags)
 * @document_start.implicit: True if the document started implicitly
 *
 * @document_end: Document end information, it is valid when
 *                &fy_event->type is &enum FYET_DOCUMENT_END
 * @document_end.document_end: The token that ended the document, or
 *                             NULL if the document was implicitly ended
 * @document_end.implicit: True if the document ended implicitly
 *
 * @alias: Alias information, it is valid when
 *         &fy_event->type is &enum FYET_ALIAS
 * @alias.anchor: The anchor token definining this alias.
 *
 * @scalar: Scalar information, it is valid when
 *          &fy_event->type is &enum FYET_SCALAR
 * @scalar.anchor: anchor token or NULL
 * @scalar.tag: tag token or NULL
 * @scalar.value: scalar value token (cannot be NULL)
 * @scalar.tag_implicit: true if the tag was implicit or explicit
 *
 * @sequence_start: Sequence start information, it is valid when
 *                  &fy_event->type is &enum FYET_SEQUENCE_START
 * @sequence_start.anchor: anchor token or NULL
 * @sequence_start.tag: tag token or NULL
 * @sequence_start.sequence_start: sequence start value token or NULL if
 *                                 the sequence was started implicitly
 * @sequence_end: Sequence end information, it is valid when
 *                &fy_event->type is &enum FYET_SEQUENCE_END
 * @sequence_end.sequence_end: The token that ended the sequence, or
 *                             NULL if the sequence was implicitly ended
 *
 * @mapping_start: Sequence start information, it is valid when
 *                 &fy_event->type is &enum FYET_MAPPING_START
 * @mapping_start.anchor: anchor token or NULL
 * @mapping_start.tag: tag token or NULL
 * @mapping_start.mapping_start: mapping start value token or NULL if
 *                                 the mapping was started implicitly
 * @mapping_end: Sequence end information, it is valid when
 *               &fy_event->type is &enum FYET_MAPPING_END
 * @mapping_end.mapping_end: The token that ended the mapping, or
 *                             NULL if the mapping was implicitly ended
 */
struct fy_event {
	enum fy_event_type type;
	/* anonymous union */
	union {
		struct {
			struct fy_token *stream_start;
		} stream_start;

		struct {
			struct fy_token *stream_end;
		} stream_end;

		struct {
			struct fy_token *document_start;
			struct fy_document_state *document_state;
			bool implicit;
		} document_start;

		struct {
			struct fy_token *document_end;
			bool implicit;
		} document_end;

		struct {
			struct fy_token *anchor;
		} alias;

		struct {
			struct fy_token *anchor;
			struct fy_token *tag;
			struct fy_token *value;
			bool tag_implicit;
		} scalar;

		struct {
			struct fy_token *anchor;
			struct fy_token *tag;
			struct fy_token *sequence_start;
		} sequence_start;

		struct {
			struct fy_token *sequence_end;
		} sequence_end;

		struct {
			struct fy_token *anchor;
			struct fy_token *tag;
			struct fy_token *mapping_start;
		} mapping_start;

		struct {
			struct fy_token *mapping_end;
		} mapping_end;
	};
};

/**
 * fy_library_version() - Return the library version string
 *
 * Returns:
 * A pointer to a version string of the form
 * <MAJOR>.<MINOR>[[.<PATCH>][-EXTRA-VERSION-INFO]]
 */
const char *fy_library_version(void);

/**
 * fy_document_event_is_implicit() - Check whether the given document event is an implicit one
 *
 * @fye: A pointer to a &struct fy_event to check.
 *       It must be either a document start or document end event.
 *
 * Returns:
 * true if the event is an implicit one.
 */
bool fy_document_event_is_implicit(const struct fy_event *fye);

/**
 * fy_parser_create() - Create a parser.
 *
 * Creates a parser with its configuration @cfg
 * The parser may be destroyed by a corresponding call to
 * fy_parser_destroy().
 *
 * @cfg: The configuration for the parser
 *
 * Returns:
 * A pointer to the parser or NULL in case of an error.
 */
struct fy_parser *fy_parser_create(const struct fy_parse_cfg *cfg);

/**
 * fy_parser_destroy() - Destroy the given parser
 *
 * @fyp: The parser to destroy
 *
 * Destroy a parser created earlier via fy_parser_create().
 */
void fy_parser_destroy(struct fy_parser *fyp);

/**
 * fy_parser_set_input_file() - Set the parser to process the given file
 *
 * Point the parser to the given @file for processing. The file
 * is located by honoring the search path of the configuration set
 * by the earlier call to fy_parser_create().
 * While the parser is in use the file will must be available.
 *
 * @fyp: The parser
 * @file: The file to parse.
 *
 * Returns:
 * zero on success, -1 on error
 */
int fy_parser_set_input_file(struct fy_parser *fyp, const char *file);

/**
 * fy_parser_set_string() - Set the parser to process the given string.
 *
 * Point the parser to the given (NULL terminated) string. Note that
 * while the parser is active the string must not go out of scope.
 *
 * @fyp: The parser
 * @str: The YAML string to parse.
 *
 * Returns:
 * zero on success, -1 on error
 */
int fy_parser_set_string(struct fy_parser *fyp, const char *str);

/**
 * fy_parser_set_input_fp() - Set the parser to process the given file
 *
 * Point the parser to use @fp for processing.
 *
 * @fyp: The parser
 * @name: The label of the stream
 * @fp: The FILE pointer, it must be open in read mode.
 *
 * Returns:
 * zero on success, -1 on error
 */
int fy_parser_set_input_fp(struct fy_parser *fyp, const char *name, FILE *fp);

/**
 * fy_parser_parse() - Parse and return the next event.
 *
 * Each call to fy_parser_parse() returns the next event from
 * the configured input of the parser, or NULL at the end of
 * the stream. The returned event must be released via
 * a matching call to fy_parser_event_free().
 *
 * @fyp: The parser
 *
 * Returns:
 * The next event in the stream or NULL.
 */
struct fy_event *fy_parser_parse(struct fy_parser *fyp);

/**
 * fy_parser_event_free() - Free an event
 *
 * Free a previously returned event from fy_parser_parse().
 *
 * @fyp: The parser
 * @fye: The event to free (may be NULL)
 */
void fy_parser_event_free(struct fy_parser *fyp, struct fy_event *fye);

/**
 * fy_parser_alloc() - Allocate memory using the parser allocator
 *
 * Allocate a block of memory using the configured parser allocator.
 * When the parser is destroyed that memory will be automatically freed.
 * Free the memory via a call to fy_parser_free().
 *
 * @fyp: The parser
 * @size: The size of memory to allocate.
 *
 * Returns:
 * A block of memory with the given size, or NULL in case of an error.
 */
void *fy_parser_alloc(struct fy_parser *fyp, size_t size);

/**
 * fy_parser_free() - Free a memory block allocated previously
 *
 * Free the memory block previously allocated via a call to fy_parser_alloc().
 *
 * @fyp: The parser
 * @data: The pointer to the memory block to free.
 */
void fy_parser_free(struct fy_parser *fyp, void *data);

/**
 * fy_parser_get_stream_error() - Check the parser for stream errors
 *
 * @fyp: The parser
 *
 * Returns:
 * true in case of a stream error, false otherwise.
 */
bool fy_parser_get_stream_error(struct fy_parser *fyp);

/**
 * fy_token_scalar_style() - Get the style of a scalar token
 *
 * @fyt: The scalar token to get it's style. Note that a NULL
 *       token is a &enum FYSS_PLAIN.
 *
 * Returns:
 * The scalar style of the token, or FYSS_PLAIN on each other case
 */
enum fy_scalar_style fy_token_scalar_style(struct fy_token *fyt);

/**
 * fy_token_get_text() - Get text (and length of it) of a token
 *
 * This method will return a pointer to the text of a token
 * along with the length of it. Note that this text is *not*
 * NULL terminated. If you need a NULL terminated pointer
 * use fy_token_get_text0().
 *
 * In case that the token is 'simple' enough (i.e. a plain scalar)
 * or something similar the returned pointer is a direct pointer
 * to the space of the parser input that created the token.
 *
 * That means that the pointer is *not* guaranteed to be valid
 * after the parser is destroyed.
 *
 * If the token is 'complex' enough, then space shall be allocated
 * out of the parser that the token belongs to, will be filled
 * and returned.
 *
 * Note that the concept of 'simple' and 'complex' is vague, and
 * that's on purpose.
 *
 * @fyt: The token out of which the text pointer will be returned.
 * @lenp: Pointer to a variable that will hold the returned length
 *
 * Returns:
 * A pointer to the text representation of the token, while
 * @lenp will be assigned the character length of said representation.
 * NULL in case of an error.
 */
const char *fy_token_get_text(struct fy_token *fyt, size_t *lenp);

/**
 * fy_token_get_text0() - Get zero terminated text of a token
 *
 * This method will return a pointer to the text of a token
 * which is zero terminated. It will allocate memory to hold
 * it in the token structure so try to use fy_token_get_text()
 * instead if possible.
 *
 * @fyt: The token out of which the text pointer will be returned.
 *
 * Returns:
 * A pointer to a zero terminated text representation of the token.
 * NULL in case of an error.
 */
const char *fy_token_get_text0(struct fy_token *fyt);

/**
 * fy_token_get_text_length() - Get length of the text of a token
 *
 * This method will return the length of the text representation
 * of a token.
 *
 * @fyt: The token
 *
 * Returns:
 * The size of the text representation of a token
 * A zero length is a valid return.
 */
size_t fy_token_get_text_length(struct fy_token *fyt);

/**
 * fy_parse_load_document() - Parse the next document from the parser stream
 *
 * This method performs parsing on a parser stream and returns the next
 * document. This means that for a compound document with multiple
 * documents, each call will return the next document.
 *
 * @fyp: The parser
 *
 * Returns:
 * The next document from the parser stream.
 */
struct fy_document *fy_parse_load_document(struct fy_parser *fyp);

/**
 * fy_parse_document_destroy() - Destroy a document created by fy_parse_load_document()
 *
 * @fyp: The parser
 * @fyd: The document to destroy
 */
void fy_parse_document_destroy(struct fy_parser *fyp, struct fy_document *fyd);

/**
 * fy_document_resolve() - Resolve anchors and merge keys
 *
 * This method performs resolution of the given document,
 * by replacing references to anchors with their contents
 * and handling merge keys (<<)
 *
 * @fyd: The document to resolve
 *
 * Returns:
 * zero on success, -1 on error
 */
int fy_document_resolve(struct fy_document *fyd);

/**
 * fy_document_has_directives() - Document directive check
 *
 * Checks whether the given document has any directives, i.e.
 * %TAG or %VERSION.
 *
 * @fyd: The document to check for directives existence
 *
 * Returns:
 * true if directives exist, false if not
 */
bool fy_document_has_directives(const struct fy_document *fyd);

/**
 * fy_document_has_explicit_document_start() - Explicit document start check
 *
 * Checks whether the given document has an explicit document start marker,
 * i.e. ---
 *
 * @fyd: The document to check for explicit start marker
 *
 * Returns:
 * true if document has an explicit document start marker, false if not
 */
bool fy_document_has_explicit_document_start(const struct fy_document *fyd);

/**
 * fy_document_has_explicit_document_end() - Explicit document end check
 *
 * Checks whether the given document has an explicit document end marker,
 * i.e. ...
 *
 * @fyd: The document to check for explicit end marker
 *
 * Returns:
 * true if document has an explicit document end marker, false if not
 */
bool fy_document_has_explicit_document_end(const struct fy_document *fyd);

/*
 * enum fy_emitter_write_type - Type of the emitted output
 *
 * Describes the kind of emitted output, which makes it
 * possible to colorize the output, or do some other content related
 * filtering.
 *
 * @fyewt_document_indicator: Output chunk is a document indicator
 * @fyewt_tag_directive: Output chunk is a tag directive
 * @fyewt_version_directive: Output chunk is a version directive
 * @fyewt_indent: Output chunk is a document indicator
 * @fyewt_indicator: Output chunk is an indicator
 * @fyewt_whitespace: Output chunk is white space
 * @fyewt_plain_scalar: Output chunk is a plain scalar
 * @fyewt_single_quoted_scalar: Output chunk is a single quoted scalar
 * @fyewt_double_quoted_scalar: Output chunk is a double quoted scalar
 * @fyewt_literal_scalar: Output chunk is a literal block scalar
 * @fyewt_folded_scalar: Output chunk is a folded block scalar
 * @fyewt_anchor: Output chunk is an anchor
 * @fyewt_tag: Output chunk is a tag
 * @fyewt_linebreak: Output chunk is a linebreak
 * @fyewt_alias: Output chunk is an alias
 * @fyewt_terminating_zero: Output chunk is a terminating zero
 * @fyewt_plain_scalar_key: Output chunk is an plain scalar key
 * @fyewt_single_quoted_scalar_key: Output chunk is an single quoted scalar key
 * @fyewt_double_quoted_scalar_key: Output chunk is an double quoted scalar key
 * @fyewt_comment: Output chunk is a comment
 *
 */
enum fy_emitter_write_type {
	fyewt_document_indicator,
	fyewt_tag_directive,
	fyewt_version_directive,
	fyewt_indent,
	fyewt_indicator,
	fyewt_whitespace,
	fyewt_plain_scalar,
	fyewt_single_quoted_scalar,
	fyewt_double_quoted_scalar,
	fyewt_literal_scalar,
	fyewt_folded_scalar,
	fyewt_anchor,
	fyewt_tag,
	fyewt_linebreak,
	fyewt_alias,
	fyewt_terminating_zero,
	fyewt_plain_scalar_key,
	fyewt_single_quoted_scalar_key,
	fyewt_double_quoted_scalar_key,
	fyewt_comment,
};

#define FYECF_INDENT_SHIFT	8
#define FYECF_INDENT_MASK	0xf
#define FYECF_INDENT(x)	(((x) & FYECF_INDENT_MASK) << FYECF_INDENT_SHIFT)

#define FYECF_WIDTH_SHIFT	12
#define FYECF_WIDTH_MASK	0xff
#define FYECF_WIDTH(x)		(((x) & FYECF_WIDTH_MASK) << FYECF_WIDTH_SHIFT)

#define FYECF_MODE_SHIFT	20
#define FYECF_MODE_MASK		0xf
#define FYECF_MODE(x)		(((x) & FYECF_MODE_MASK) << FYECF_MODE_SHIFT)

#define FYECF_DOC_START_MARK_SHIFT	24
#define FYECF_DOC_START_MARK_MASK	0x3
#define FYECF_DOC_START_MARK(x)		(((x) & FYECF_DOC_START_MARK_MASK) << FYECF_DOC_START_MARK_SHIFT)

#define FYECF_DOC_END_MARK_SHIFT	26
#define FYECF_DOC_END_MARK_MASK		0x3
#define FYECF_DOC_END_MARK(x)		(((x) & FYECF_DOC_END_MARK_MASK) << FYECF_DOC_END_MARK_SHIFT)

#define FYECF_VERSION_DIR_SHIFT		28
#define FYECF_VERSION_DIR_MASK		0x3
#define FYECF_VERSION_DIR(x)		(((x) & FYECF_VERSION_DIR_MASK) << FYECF_VERSION_DIR_SHIFT)

#define FYECF_TAG_DIR_SHIFT		30
#define FYECF_TAG_DIR_MASK		0x3
#define FYECF_TAG_DIR(x)		(((x) & FYECF_TAG_DIR_MASK) << FYECF_TAG_DIR_SHIFT)

/**
 * enum fy_emitter_cfg_flags - Emitter configuration flags
 *
 * These flags control the operation of the emitter
 *
 * @FYECF_SORT_KEYS: Sort key when emitting
 * @FYECF_OUTPUT_COMMENTS: Output comments (experimental)
 * @FYECF_INDENT_DEFAULT: Default emit output indent
 * @FYECF_INDENT_1: Output indent is 1
 * @FYECF_INDENT_2: Output indent is 2
 * @FYECF_INDENT_3: Output indent is 3
 * @FYECF_INDENT_4: Output indent is 4
 * @FYECF_INDENT_5: Output indent is 5
 * @FYECF_INDENT_6: Output indent is 6
 * @FYECF_INDENT_7: Output indent is 7
 * @FYECF_INDENT_8: Output indent is 8
 * @FYECF_INDENT_9: Output indent is 9
 * @FYECF_WIDTH_DEFAULT: Default emit output width
 * @FYECF_WIDTH_80: Output width is 80
 * @FYECF_WIDTH_132: Output width is 132
 * @FYECF_WIDTH_INF: Output width is infinite
 * @FYECF_MODE_ORIGINAL: Emit using the same flow mode as the original
 * @FYECF_MODE_BLOCK: Emit using only the block mode
 * @FYECF_MODE_FLOW: Emit using only the flow mode
 * @FYECF_MODE_FLOW_ONELINE: Emit using only the flow mode (in one line)
 * @FYECF_MODE_JSON: Emit using JSON mode (non type preserving)
 * @FYECF_MODE_JSON_TP: Emit using JSON mode (type preserving)
 * @FYECF_MODE_JSON_ONELINE: Emit using JSON mode (non type preserving, one line)
 * @FYECF_DOC_START_MARK_AUTO: Automatically generate document start markers if required
 * @FYECF_DOC_START_MARK_OFF: Do not generate document start markers
 * @FYECF_DOC_START_MARK_ON: Always generate document start markers
 * @FYECF_DOC_END_MARK_AUTO: Automatically generate document end markers if required
 * @FYECF_DOC_END_MARK_OFF: Do not generate document end markers
 * @FYECF_DOC_END_MARK_ON: Always generate document end markers
 * @FYECF_VERSION_DIR_AUTO: Automatically generate version directive
 * @FYECF_VERSION_DIR_OFF: Never generate version directive
 * @FYECF_VERSION_DIR_ON: Always generate version directive
 * @FYECF_TAG_DIR_AUTO: Automatically generate tag directives
 * @FYECF_TAG_DIR_OFF: Never generate tag directives
 * @FYECF_TAG_DIR_ON: Always generate tag directives
 * @FYECF_DEFAULT: The default emitter configuration
 */
enum fy_emitter_cfg_flags {
	FYECF_SORT_KEYS			= FY_BIT(0),
	FYECF_OUTPUT_COMMENTS		= FY_BIT(1),
	FYECF_INDENT_DEFAULT		= FYECF_INDENT(0),
	FYECF_INDENT_1			= FYECF_INDENT(1),
	FYECF_INDENT_2			= FYECF_INDENT(2),
	FYECF_INDENT_3			= FYECF_INDENT(3),
	FYECF_INDENT_4			= FYECF_INDENT(4),
	FYECF_INDENT_5			= FYECF_INDENT(5),
	FYECF_INDENT_6			= FYECF_INDENT(6),
	FYECF_INDENT_7			= FYECF_INDENT(7),
	FYECF_INDENT_8			= FYECF_INDENT(8),
	FYECF_INDENT_9			= FYECF_INDENT(9),
	FYECF_WIDTH_DEFAULT		= FYECF_WIDTH(80),
	FYECF_WIDTH_80			= FYECF_WIDTH(80),
	FYECF_WIDTH_132			= FYECF_WIDTH(132),
	FYECF_WIDTH_INF			= FYECF_WIDTH(255),
	FYECF_MODE_ORIGINAL		= FYECF_MODE(0),
	FYECF_MODE_BLOCK		= FYECF_MODE(1),
	FYECF_MODE_FLOW			= FYECF_MODE(2),
	FYECF_MODE_FLOW_ONELINE 	= FYECF_MODE(3),
	FYECF_MODE_JSON			= FYECF_MODE(4),
	FYECF_MODE_JSON_TP		= FYECF_MODE(5),
	FYECF_MODE_JSON_ONELINE 	= FYECF_MODE(6),
	FYECF_DOC_START_MARK_AUTO	= FYECF_DOC_START_MARK(0),
	FYECF_DOC_START_MARK_OFF	= FYECF_DOC_START_MARK(1),
	FYECF_DOC_START_MARK_ON		= FYECF_DOC_START_MARK(2),
	FYECF_DOC_END_MARK_AUTO		= FYECF_DOC_END_MARK(0),
	FYECF_DOC_END_MARK_OFF		= FYECF_DOC_END_MARK(1),
	FYECF_DOC_END_MARK_ON		= FYECF_DOC_END_MARK(2),
	FYECF_VERSION_DIR_AUTO		= FYECF_VERSION_DIR(0),
	FYECF_VERSION_DIR_OFF		= FYECF_VERSION_DIR(1),
	FYECF_VERSION_DIR_ON		= FYECF_VERSION_DIR(2),
	FYECF_TAG_DIR_AUTO		= FYECF_TAG_DIR(0),
	FYECF_TAG_DIR_OFF		= FYECF_TAG_DIR(1),
	FYECF_TAG_DIR_ON		= FYECF_TAG_DIR(2),

	FYECF_DEFAULT			= FYECF_WIDTH_INF |
					  FYECF_MODE_ORIGINAL |
					  FYECF_INDENT_DEFAULT,
};

/**
 * struct fy_emitter_cfg - emitter configuration structure.
 *
 * Argument to the fy_emitter_create() method which
 * is the way to convert a runtime document structure back to YAML.
 *
 * @flags: Configuration flags
 * @output: Pointer to the method that will perform output.
 * @userdata: Opaque user data pointer
 */
struct fy_emitter_cfg {
	enum fy_emitter_cfg_flags flags;
	int (*output)(struct fy_emitter *emit, enum fy_emitter_write_type type,
		      const char *str, int len, void *userdata);
	void *userdata;
};

/**
 * fy_emitter_get_cfg() - Get the configuration of an emitter
 *
 * @emit: The emitter
 *
 * Returns:
 * The configuration of the emitter
 */
const struct fy_emitter_cfg *fy_emitter_get_cfg(struct fy_emitter *emit);

/**
 * fy_emitter_create() - Create an emitter
 *
 * Creates an emitter using the supplied configuration
 *
 * @cfg: The emitter configuration
 *
 * Returns:
 * The newly created emitter or NULL on error.
 */
struct fy_emitter *fy_emitter_create(struct fy_emitter_cfg *cfg);

/**
 * fy_emitter_destroy() - Destroy an emitter
 *
 * Destroy an emitter previously created by fy_emitter_create()
 *
 * @emit: The emitter to destroy
 */
void fy_emitter_destroy(struct fy_emitter *emit);

/**
 * fy_emit_document() - Emit the document using the emitter
 *
 * Emits a document in YAML format using the emitter.
 *
 * @emit: The emitter
 * @fyd: The document to emit
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_document(struct fy_emitter *emit, struct fy_document *fyd);

/**
 * fy_emit_document_start() - Emit document start using the emitter
 *
 * Emits a document start using the emitter. This is used in case
 * you need finer control over the emitting output.
 *
 * @emit: The emitter
 * @fyd: The document to use for emitting it's start
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_document_start(struct fy_emitter *emit, struct fy_document *fyd);

/**
 * fy_emit_document_end() - Emit document end using the emitter
 *
 * Emits a document end using the emitter. This is used in case
 * you need finer control over the emitting output.
 *
 * @emit: The emitter
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_document_end(struct fy_emitter *emit);

/**
 * fy_emit_node() - Emit a single node using the emitter
 *
 * Emits a single node using the emitter. This is used in case
 * you need finer control over the emitting output.
 *
 * @emit: The emitter
 * @fyn: The node to emit
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_node(struct fy_emitter *emit, struct fy_node *fyn);

/**
 * fy_emit_root_node() - Emit a single root node using the emitter
 *
 * Emits a single root node using the emitter. This is used in case
 * you need finer control over the emitting output.
 *
 * @emit: The emitter
 * @fyn: The root node to emit
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_root_node(struct fy_emitter *emit, struct fy_node *fyn);

/**
 * fy_emit_explicit_document_end() - Emit an explicit document end
 *
 * Emits an explicit document end, i.e. ... . Use this if you
 * you need finer control over the emitting output.
 *
 * @emit: The emitter
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_explicit_document_end(struct fy_emitter *emit);

/**
 * fy_emit_document_to_fp() - Emit a document to an file pointer
 *
 * Emits a document from the root to the given file pointer.
 *
 * @fyd: The document to emit
 * @flags: The emitter flags to use
 * @fp: The file pointer to output to
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_document_to_fp(struct fy_document *fyd,
			   enum fy_emitter_cfg_flags flags, FILE *fp);

/**
 * fy_emit_document_to_file() - Emit a document to file
 *
 * Emits a document from the root to the given file.
 * The file will be fopen'ed using a "wa" mode.
 *
 * @fyd: The document to emit
 * @flags: The emitter flags to use
 * @filename: The filename to output to, or NULL for stdout
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_emit_document_to_file(struct fy_document *fyd,
			     enum fy_emitter_cfg_flags flags,
			     const char *filename);

/**
 * fy_emit_document_to_buffer() - Emit a document to a buffer
 *
 * Emits an document from the root to the given buffer.
 * If the document does not fit, an error will be returned.
 *
 * @fyd: The document to emit
 * @flags: The emitter flags to use
 * @buf: Pointer to the buffer area to fill
 * @size: Size of the buffer
 *
 * Returns:
 * A positive number, which is the size of the emitted document
 * on the buffer on success, -1 on error
 */
int fy_emit_document_to_buffer(struct fy_document *fyd,
			       enum fy_emitter_cfg_flags flags,
			       char *buf, int size);

/**
 * fy_emit_document_to_string() - Emit a document to an allocated string
 *
 * Emits an document from the root to a string which will be dynamically
 * allocated.
 *
 * @fyd: The document to emit
 * @flags: The emitter flags to use
 *
 * Returns:
 * A pointer to the allocated string, or NULL in case of an error
 */
char *fy_emit_document_to_string(struct fy_document *fyd, enum fy_emitter_cfg_flags flags);

/**
 * fy_emit_node_to_buffer() - Emit a node (recursively) to a buffer
 *
 * Emits a node recursively to the given buffer.
 * If the document does not fit, an error will be returned.
 *
 * @fyn: The node to emit
 * @flags: The emitter flags to use
 * @buf: Pointer to the buffer area to fill
 * @size: Size of the buffer
 *
 * Returns:
 * A positive number, which is the size of the emitted node
 * on the buffer on success, -1 on error
 */
int fy_emit_node_to_buffer(struct fy_node *fyn, enum fy_emitter_cfg_flags flags, char *buf, int size);

/**
 * fy_emit_node_to_string() - Emit a node to an allocated string
 *
 * Emits a node recursively to a string which will be dynamically
 * allocated.
 *
 * @fyn: The node to emit
 * @flags: The emitter flags to use
 *
 * Returns:
 * A pointer to the allocated string, or NULL in case of an error
 */
char *fy_emit_node_to_string(struct fy_node *fyn, enum fy_emitter_cfg_flags flags);

/**
 * fy_node_copy() - Copy a node, associating the new node with the given document
 *
 * Make a deep copy of a node, associating the copy with the given document.
 * Note that no content copying takes place as the contents of the nodes
 * are reference counted. This means that the operation is relatively inexpensive.
 *
 * Note that the copy includes all anchors contained in the subtree of the
 * source, so this call will register them with the document.
 *
 * @fyd: The document which the resulting node will be associated with
 * @fyn_from: The source node to recursively copy
 *
 * Returns:
 * The copied node on success, NULL on error
 */
struct fy_node *fy_node_copy(struct fy_document *fyd, struct fy_node *fyn_from);

/**
 * fy_node_insert() - Insert a node to the given node
 *
 * Insert a node to another node. If @fyn_from is NULL then this
 * operation will delete the @fyn_to node.
 *
 * The operation varies according to the types of the arguments:
 *
 * from: scalar
 *
 * to: another-scalar -> scalar
 * to: { key: value } -> scalar
 * to: [ seq0, seq1 ] -> scalar
 *
 * from: [ seq2 ]
 * to: scalar -> [ seq2 ]
 * to: { key: value } -> [ seq2 ]
 * to: [ seq0, seq1 ] -> [ seq0, seq1, sec2 ]
 *
 * from: { another-key: another-value }
 * to: scalar -> { another-key: another-value }
 * to: { key: value } -> { key: value, another-key: another-value }
 * to: [ seq0, seq1 ] -> { another-key: another-value }
 *
 * from: { key: another-value }
 * to: scalar -> { key: another-value }
 * to: { key: value } -> { key: another-value }
 * to: [ seq0, seq1 ] -> { key: another-value }
 *
 * The rules are:
 * - If target node changes type, source ovewrites target.
 * - If source or target node are scalars, source it overwrites target.
 * - If target and source are sequences the items of the source sequence
 *   are appended to the target sequence.
 * - If target and source are maps the key, value pairs of the source
 *   are appended to the target map. If the target map contains a
 *   key-value pair that is present in the source map, it is overwriten
 *   by it.
 *
 * @fyn_to: The target node
 * @fyn_from: The source node
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_insert(struct fy_node *fyn_to, struct fy_node *fyn_from);

/**
 * fy_document_insert_at() - Insert a node to the given path in the document
 *
 * Insert a node to a given point in the document. If @fyn is NULL then this
 * operation will delete the target node.
 *
 * Please see fy_node_insert for details of operation.
 *
 * Note that in any case the fyn node will be unref'ed.
 * So if the operation fails, and the reference is 0
 * the node will be freed. If you want it to stick around
 * take a reference before.
 *
 * @fyd: The document
 * @path: The path where the insert operation will target
 * @fyn: The source node
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_document_insert_at(struct fy_document *fyd,
			  const char *path, struct fy_node *fyn);

/**
 * enum fy_node_type - Node type
 *
 * Each node may be one of the following types
 *
 * @FYNT_SCALAR: Node is a scalar
 * @FYNT_SEQUENCE: Node is a sequence
 * @FYNT_MAPPING: Node is a mapping
 */
enum fy_node_type {
	FYNT_SCALAR,
	FYNT_SEQUENCE,
	FYNT_MAPPING,
};

/**
 * enum fy_node_style - Node style
 *
 * A node may contain a hint of how it should be
 * rendered, encoded as a style.
 *
 * @FYNS_ANY: No hint, let the emitter decide
 * @FYNS_FLOW: Prefer flow style (for sequence/mappings)
 * @FYNS_BLOCK: Prefer block style (for sequence/mappings)
 * @FYNS_PLAIN: Plain style preferred
 * @FYNS_SINGLE_QUOTED: Single quoted style preferred
 * @FYNS_DOUBLE_QUOTED: Double quoted style preferred
 * @FYNS_LITERAL: Literal style preferred (valid in block context)
 * @FYNS_FOLDED: Folded style preferred (valid in block context)
 * @FYNS_ALIAS: It's an alias
 */
enum fy_node_style {
	FYNS_ANY = -1,
	FYNS_FLOW,
	FYNS_BLOCK,
	FYNS_PLAIN,
	FYNS_SINGLE_QUOTED,
	FYNS_DOUBLE_QUOTED,
	FYNS_LITERAL,
	FYNS_FOLDED,
	FYNS_ALIAS,
};

/**
 * fy_node_style_from_scalar_style() - Convert from scalar to node style
 *
 * Convert a scalar style to a node style.
 *
 * @sstyle: The input scalar style
 *
 * Returns:
 * The matching node style
 */
static inline enum fy_node_style
fy_node_style_from_scalar_style(enum fy_scalar_style sstyle)
{
	if (sstyle == FYSS_ANY)
		return FYNS_ANY;
	return FYNS_PLAIN + (sstyle - FYSS_PLAIN);
}

/**
 * fy_node_compare() - Compare two nodes for equality
 *
 * Compare two nodes for equality.
 * The comparison is 'deep', i.e. it recurses in subnodes,
 * and orders the keys of maps using default libc strcmp
 * ordering. For scalar the comparison is performed after
 * any escaping so it's a true content comparison.
 *
 * @fyn1: The first node to use in the comparison
 * @fyn2: The second node to use in the comparison
 *
 * Returns:
 * true if the nodes contain the same content, false otherwise
 */
bool fy_node_compare(struct fy_node *fyn1, struct fy_node *fyn2);

/**
 * fy_node_compare_string() - Compare a node for equality with a YAML string
 *
 * Compare a node for equality with a YAML string.
 * The comparison is performed using fy_node_compare() with the
 * first node supplied as an argument and the second being generated
 * by calling fy_document_build_from_string with the YAML string.
 *
 * @fyn: The node to use in the comparison
 * @str: The YAML string to compare against
 *
 * Returns:
 * true if the node and the string are equal.
 */
bool fy_node_compare_string(struct fy_node *fyn, const char *str);

/**
 * fy_document_create() - Create an empty document
 *
 * Create an empty document using the provided parser configuration.
 * If NULL use the default parse configuration.
 *
 * @cfg: The parse configuration to use or NULL for the default.
 *
 * Returns:
 * The created empty document, or NULL on error.
 */
struct fy_document *fy_document_create(const struct fy_parse_cfg *cfg);

/**
 * fy_document_destroy() - Destroy a document previously created via
 *                         fy_document_create()
 *
 * Destroy a document (along with all children documents)
 *
 * @fyd: The document to destroy
 *
 */
void fy_document_destroy(struct fy_document *fyd);

/**
 * fy_document_set_parent() - Make a document a child of another
 *
 * Set the parent of @fyd_child document to be @fyd
 *
 * @fyd: The parent document
 * @fyd_child: The child document
 *
 * Returns:
 * 0 if all OK, -1 on error.
 */
int fy_document_set_parent(struct fy_document *fyd, struct fy_document *fyd_child);

/**
 * fy_document_build_from_string() - Create a document using the provided YAML source.
 *
 * Create a document parsing the provided string as a YAML source.
 *
 * @cfg: The parse configuration to use or NULL for the default.
 * @str: The YAML source to use.
 *
 * Returns:
 * The created document, or NULL on error.
 */
struct fy_document *fy_document_build_from_string(const struct fy_parse_cfg *cfg, const char *str);

/**
 * fy_document_build_from_file() - Create a document parsing the given file
 *
 * Create a document parsing the provided file as a YAML source.
 *
 * @cfg: The parse configuration to use or NULL for the default.
 * @file: The name of the file to parse
 *
 * Returns:
 * The created document, or NULL on error.
 */
struct fy_document *fy_document_build_from_file(const struct fy_parse_cfg *cfg, const char *file);

/**
 * fy_document_build_from_fp() - Create a document parsing the given file pointer
 *
 * Create a document parsing the provided file pointer as a YAML source.
 *
 * @cfg: The parse configuration to use or NULL for the default.
 * @fp: The file pointer
 *
 * Returns:
 * The created document, or NULL on error.
 */
struct fy_document *fy_document_build_from_fp(const struct fy_parse_cfg *cfg, FILE *fp);

/**
 * fy_document_vbuildf() - Create a document using the provided YAML via vprintf formatting
 *
 * Create a document parsing the provided string as a YAML source. The string
 * is created by applying vprintf formatting.
 *
 * @cfg: The parse configuration to use or NULL for the default.
 * @fmt: The format string creating the YAML source to use.
 * @ap: The va_list argument pointer
 *
 * Returns:
 * The created document, or NULL on error.
 */
struct fy_document *fy_document_vbuildf(const struct fy_parse_cfg *cfg, const char *fmt, va_list ap);

/**
 * fy_document_buildf() - Create a document using the provided YAML source via printf formatting
 *
 * Create a document parsing the provided string as a YAML source. The string
 * is created by applying printf formatting.
 *
 * @cfg: The parse configuration to use or NULL for the default.
 * @fmt: The format string creating the YAML source to use.
 * @...: The printf arguments 
 *
 * Returns:
 * The created document, or NULL on error.
 */
struct fy_document *fy_document_buildf(const struct fy_parse_cfg *cfg, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));

/**
 * fy_document_root() - Return the root node of the document
 *
 * Returns the root of the document. If the document is empty
 * NULL will be returned instead.
 *
 * @fyd: The document
 *
 * Returns:
 * The root of the document, or NULL if the document is empty.
 */
struct fy_node *fy_document_root(struct fy_document *fyd);

/**
 * fy_document_set_root() - Set the root of the document
 *
 * Set the root of a document. If the document was not empty
 * the old root will be freed.
 *
 * @fyd: The document
 * @fyn: The new root of the document.
 */
void fy_document_set_root(struct fy_document *fyd, struct fy_node *fyn);

/**
 * fy_node_get_type() - Get the node type
 *
 * Retrieve the node type. It is one of FYNT_SCALAR, FYNT_SEQUENCE
 * or FYNT_MAPPING. A NULL node argument is a FYNT_SCALAR.
 *
 * @fyn: The node
 *
 * Returns:
 * The node type
 */
enum fy_node_type fy_node_get_type(struct fy_node *fyn);

/**
 * fy_node_get_style() - Get the node style
 *
 * Retrieve the node rendering style.
 * If the node is NULL then the style is FYNS_PLAIN.
 *
 * @fyn: The node
 *
 * Returns:
 * The node style
 */
enum fy_node_style fy_node_get_style(struct fy_node *fyn);

/**
 * fy_node_is_scalar() - Check whether the node is a scalar
 *
 * Convenience method for checking whether a node is a scalar.
 *
 * @fyn: The node
 *
 * Returns:
 * true if the node is a scalar, false otherwise
 */
static inline bool fy_node_is_scalar(struct fy_node *fyn)
{
	return fy_node_get_type(fyn) == FYNT_SCALAR;
}

/**
 * fy_node_is_sequence() - Check whether the node is a sequence
 *
 * Convenience method for checking whether a node is a sequence.
 *
 * @fyn: The node
 *
 * Returns:
 * true if the node is a sequence, false otherwise
 */
static inline bool fy_node_is_sequence(struct fy_node *fyn)
{
	return fy_node_get_type(fyn) == FYNT_SEQUENCE;
}

/**
 * fy_node_is_mapping() - Check whether the node is a mapping
 *
 * Convenience method for checking whether a node is a mapping.
 *
 * @fyn: The node
 *
 * Returns:
 * true if the node is a mapping, false otherwise
 */
static inline bool fy_node_is_mapping(struct fy_node *fyn)
{
	return fy_node_get_type(fyn) == FYNT_MAPPING;
}

/**
 * fy_node_free() - Free a node
 *
 * Recursively Frees the given node releasing the memory it uses, removing
 * any anchors on the document it contains, and releasing references
 * on the tokens it contains.
 *
 * @fyn: The node to free
 */
void fy_node_free(struct fy_node *fyn);

/**
 * fy_node_build_from_string() - Create a node using the provided YAML source.
 *
 * Create a node parsing the provided string as a YAML source. The
 * node created will be associated with the provided document.
 *
 * @fyd: The document
 * @str: The YAML source to use.
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_build_from_string(struct fy_document *fyd, const char *str);

/**
 * fy_node_build_from_file() - Create a node using the provided YAML file.
 *
 * Create a node parsing the provided file as a YAML source. The
 * node created will be associated with the provided document.
 *
 * @fyd: The document
 * @file: The name of the file to parse
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_build_from_file(struct fy_document *fyd, const char *file);

/**
 * fy_node_build_from_fp() - Create a node using the provided file pointer.
 *
 * Create a node parsing the provided file pointer as a YAML source. The
 * node created will be associated with the provided document.
 *
 * @fyd: The document
 * @fp: The file pointer
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_build_from_fp(struct fy_document *fyd, FILE *fp);

/**
 * fy_node_vbuildf() - Create a node using the provided YAML source via vprintf formatting
 *
 * Create a node parsing the resulting string as a YAML source. The string
 * is created by applying vprintf formatting.
 *
 * @fyd: The document
 * @fmt: The format string creating the YAML source to use.
 * @ap: The va_list argument pointer
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_vbuildf(struct fy_document *fyd, const char *fmt, va_list ap);

/**
 * fy_node_buildf() - Create a node using the provided YAML source via printf formatting
 *
 * Create a node parsing the resulting string as a YAML source. The string
 * is created by applying printf formatting.
 *
 * @fyd: The document
 * @fmt: The format string creating the YAML source to use.
 * @...: The printf arguments 
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_buildf(struct fy_document *fyd, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));

/**
 * fy_node_by_path() - Retrieve a node using the provided path spec.
 *
 * This method will retrieve a node relative to the given node using
 * the provided path spec.
 *
 * Path specs are comprised of keys seperated by slashes '/'.
 * Keys are either regular YAML expressions in flow format for traversing
 * mappings, or indexes in brackets for traversing sequences.
 * Path specs may start with '/' which is silently ignored.
 *
 * A few examples will make this clear
 *
 * fyn = { foo: bar } - fy_node_by_path(fyn, "/foo") -> bar
 * fyn = [ foo, bar ] - fy_node_by_path(fyn, "[1]") -> bar
 * fyn = { { foo: bar }: baz } - fy_node_by_path(fyn, "{foo: bar}") -> baz
 * fyn = [ foo, { bar: baz } } - fy_node_by_path(fyn, "[1]/bar") -> baz
 *
 * Note that the special characters /{}[] are not escaped in plain style,
 * so you will not be able to use them as path traversal keys.
 * In that case you can easily use either the single, or double quoted forms:
 *
 * fyn = { foo/bar: baz } - fy_node_by_path(fyn, "'foo/bar'") -> baz
 *
 * @fyn: The node to use as start of the traversal operation
 * @path: The path spec to use in the traversal operation
 *
 * Returns:
 * The retrieved node, or NULL if not possible to be found.
 */
struct fy_node *fy_node_by_path(struct fy_node *fyn, const char *path);

/**
 * fy_node_get_path() - Get the path of this nod
 *
 * Retrieve the given node's path address relative to the document root.
 * The address is dynamically allocated and should be freed when
 * you're done with it.
 *
 * @fyn: The node
 *
 * Returns:
 * The node's address, or NULL if fyn is the root.
 */
char *fy_node_get_path(struct fy_node *fyn);

/**
 * fy_node_get_parent_address() - Get the path address of this node's parent
 *
 * Retrieve the given node's parent path address
 * The address is dynamically allocated and should be freed when
 * you're done with it.
 *
 * @fyn: The node
 *
 * Returns:
 * The parent's address, or NULL if fyn is the root.
 */
char *fy_node_get_parent_address(struct fy_node *fyn);

/**
 * fy_node_create_scalar() - Create a scalar node.
 *
 * Create a scalar node using the provided memory area as input.
 * The input is expected to be regular utf8 encoded. It may contain
 * escaped characters in which case the style of the scalar will be
 * set to double quoted.
 *
 * Note that the data are not copied, merely a reference is taken, so
 * it must be available while the node is in use.
 *
 * @fyd: The document which the resulting node will be associated with
 * @data: Pointer to the data area
 * @size: Size of the data area, or (size_t)-1 for '\0' terminated data.
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_create_scalar(struct fy_document *fyd, const char *data, size_t size);

/**
 * fy_node_create_sequence() - Create an empty sequence node.
 *
 * Create an empty sequence node associated with the given document.
 *
 * @fyd: The document which the resulting node will be associated with
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_create_sequence(struct fy_document *fyd);

/**
 * fy_node_create_mapping() - Create an empty mapping node.
 *
 * Create an empty mapping node associated with the given document.
 *
 * @fyd: The document which the resulting node will be associated with
 *
 * Returns:
 * The created node, or NULL on error.
 */
struct fy_node *fy_node_create_mapping(struct fy_document *fyd);

/**
 * fy_node_set_tag() - Set the tag of node
 *
 * Manually set the tag of a node. The tag must be a valid one for
 * the document the node belongs to.
 *
 * Note that the data are not copied, merely a reference is taken, so
 * it must be available while the node is in use.
 *
 * If the node already contains a tag it will be overwriten.
 *
 * @fyn: The node to set it's tag.
 * @data: Pointer to the tag data.
 * @len: Size of the tag data, or (size_t)-1 for '\0' terminated.
 *
 * Returns:
 * 0 on success, -1 on error.
 */
int fy_node_set_tag(struct fy_node *fyn, const char *data, size_t len);

/**
 * fy_node_get_tag() - Get the tag of the node
 *
 * This method will return a pointer to the text of a tag
 * along with the length of it. Note that this text is *not*
 * NULL terminated.
 *
 * @fyn: The node
 * @lenp: Pointer to a variable that will hold the returned length
 *
 * Returns:
 * A pointer to the tag of the node, while @lenp will be assigned the
 * length of said tag.
 * A NULL will be returned in case of an error.
 */
const char *fy_node_get_tag(struct fy_node *fyn, size_t *lenp);

/**
 * fy_node_get_scalar() - Get the scalar content of the node
 *
 * This method will return a pointer to the text of the scalar
 * content of a node along with the length of it.
 * Note that this pointer is *not* NULL terminated.
 *
 * @fyn: The scalar node
 * @lenp: Pointer to a variable that will hold the returned length
 *
 * Returns:
 * A pointer to the scalar content of the node, while @lenp will be assigned the
 * length of said content.
 * A NULL will be returned in case of an error, i.e. the node is not
 * a scalar.
 */
const char *fy_node_get_scalar(struct fy_node *fyn, size_t *lenp);

/**
 * fy_node_get_scalar0() - Get the scalar content of the node
 *
 * This method will return a pointer to the text of the scalar
 * content of a node as a null terminated string.
 * Note that this call will allocate memory to hold the null terminated
 * string so if possible use fy_node_get_scalar()
 *
 * @fyn: The scalar node
 *
 * Returns:
 * A pointer to the scalar content of the node or NULL in returned in case of an error.
 */
const char *fy_node_get_scalar0(struct fy_node *fyn);

/**
 * fy_node_get_scalar_length() - Get the length of the scalar content
 *
 * This method will return the size of the scalar content of the node.
 * If the node is not a scalar it will return 0.
 *
 * @fyn: The scalar node
 *
 * Returns:
 * The size of the scalar content, or 0 if node is not scalar.
 */
size_t fy_node_get_scalar_length(struct fy_node *fyn);

/**
 * fy_node_sequence_iterate() - Iterate over a sequence node
 *
 * This method iterates over all the item nodes in the sequence node.
 * The start of the iteration is signalled by a NULL in \*prevp.
 *
 * @fyn: The sequence node
 * @prevp: The previous sequence iterator
 *
 * Returns:
 * The next node in sequence or NULL at the end of the sequence.
 */
struct fy_node *fy_node_sequence_iterate(struct fy_node *fyn, void **prevp);

/**
 * fy_node_sequence_reverse_iterate() - Iterate over a sequence node in reverse
 *
 * This method iterates in reverse over all the item nodes in the sequence node.
 * The start of the iteration is signalled by a NULL in \*prevp.
 *
 * @fyn: The sequence node
 * @prevp: The previous sequence iterator
 *
 * Returns:
 * The next node in reverse sequence or NULL at the end of the sequence.
 */
struct fy_node *fy_node_sequence_reverse_iterate(struct fy_node *fyn, void **prevp);

/**
 * fy_node_sequence_item_count() - Return the item count of the sequence
 *
 * Get the item count of the sequence.
 *
 * @fyn: The sequence node
 *
 * Returns:
 * The count of items in the sequence or -1 in case of an error.
 */
int fy_node_sequence_item_count(struct fy_node *fyn);

/**
 * fy_node_sequence_get_by_index() - Return a sequence item by index
 *
 * Retrieve a node in the sequence using it's index. If index
 * is positive or zero the count is from the start of the sequence,
 * while if negative from the end. I.e. -1 returns the last item
 * in the sequence.
 *
 * @fyn: The sequence node
 * @index: The index of the node to retrieve.
 *         - >= 0 counting from the start
 *         - < 0 counting from the end
 *
 * Returns:
 * The node at the specified index or NULL if no such item exist.
 */
struct fy_node *fy_node_sequence_get_by_index(struct fy_node *fyn, int index);

/**
 * fy_node_sequence_append() - Append a node item to a sequence
 *
 * Append a node item to a sequence.
 *
 * @fyn_seq: The sequence node
 * @fyn: The node item to append
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_sequence_append(struct fy_node *fyn_seq, struct fy_node *fyn);

/**
 * fy_node_sequence_prepend() - Append a node item to a sequence
 *
 * Prepend a node item to a sequence.
 *
 * @fyn_seq: The sequence node
 * @fyn: The node item to prepend
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_sequence_prepend(struct fy_node *fyn_seq, struct fy_node *fyn);

/**
 * fy_node_sequence_insert_before() - Insert a node item before another
 *
 * Insert a node item before another in the sequence.
 *
 * @fyn_seq: The sequence node
 * @fyn_mark: The node item which the node will be inserted before.
 * @fyn: The node item to insert in the sequence.
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_sequence_insert_before(struct fy_node *fyn_seq,
				   struct fy_node *fyn_mark, struct fy_node *fyn);

/**
 * fy_node_sequence_insert_after() - Insert a node item after another
 *
 * Insert a node item after another in the sequence.
 *
 * @fyn_seq: The sequence node
 * @fyn_mark: The node item which the node will be inserted after.
 * @fyn: The node item to insert in the sequence.
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_sequence_insert_after(struct fy_node *fyn_seq,
				   struct fy_node *fyn_mark, struct fy_node *fyn);

/**
 * fy_node_sequence_remove() - Remove an item from a sequence
 *
 * Remove a node item from a sequence and return it.
 *
 * @fyn_seq: The sequence node
 * @fyn: The node item to remove from the sequence.
 *
 * Returns:
 * The removed node item fyn, or NULL in case of an error.
 */
struct fy_node *fy_node_sequence_remove(struct fy_node *fyn_seq, struct fy_node *fyn);

/**
 * fy_node_mapping_iterate() - Iterate over a mapping node
 *
 * This method iterates over all the node pairs in the mapping node.
 * The start of the iteration is signalled by a NULL in \*prevp.
 *
 * Note that while a mapping is an unordered collection of key/values
 * the order of which they are created is important for presentation
 * purposes.
 *
 * @fyn: The mapping node
 * @prevp: The previous sequence iterator
 *
 * Returns:
 * The next node pair in the mapping or NULL at the end of the mapping.
 */
struct fy_node_pair *fy_node_mapping_iterate(struct fy_node *fyn, void **prevp);

/**
 * fy_node_mapping_reverse_iterate() - Iterate over a mapping node in reverse
 *
 * This method iterates in reverse over all the node pairs in the mapping node.
 * The start of the iteration is signalled by a NULL in \*prevp.
 *
 * Note that while a mapping is an unordered collection of key/values
 * the order of which they are created is important for presentation
 * purposes.
 *
 * @fyn: The mapping node
 * @prevp: The previous sequence iterator
 *
 * Returns:
 * The next node pair in reverse sequence in the mapping or NULL at the end of the mapping.
 */
struct fy_node_pair *fy_node_mapping_reverse_iterate(struct fy_node *fyn, void **prevp);

/**
 * fy_node_mapping_item_count() - Return the node pair count of the mapping
 *
 * Get the count of the node pairs in the mapping.
 *
 * @fyn: The mapping node
 *
 * Returns:
 * The count of node pairs in the mapping or -1 in case of an error.
 */
int fy_node_mapping_item_count(struct fy_node *fyn);

/**
 * fy_node_mapping_get_by_index() - Return a node pair by index
 *
 * Retrieve a node pair in the mapping using its index. If index
 * is positive or zero the count is from the start of the sequence,
 * while if negative from the end. I.e. -1 returns the last node pair
 * in the mapping.
 *
 * @fyn: The mapping node
 * @index: The index of the node pair to retrieve.
 *         - >= 0 counting from the start
 *         - < 0 counting from the end
 *
 * Returns:
 * The node pair at the specified index or NULL if no such item exist.
 */
struct fy_node_pair *fy_node_mapping_get_by_index(struct fy_node *fyn, int index);

/**
 * fy_node_mapping_lookup_by_string() - Lookup a node value in mapping by string
 *
 * This method will return the value of node pair that contains the same key
 * from the YAML node created from the @key argument. The comparison of the
 * node is using fy_node_compare()
 *
 * @fyn: The mapping node
 * @key: The YAML source to use as key
 *
 * Returns:
 * The value matching the given key, or NULL if not found.
 */
struct fy_node *fy_node_mapping_lookup_by_string(struct fy_node *fyn, const char *key);

/**
 * fy_node_mapping_lookup_pair() - Lookup a node pair matching the provided key
 *
 * This method will return the node pair that matches the provided @fyn_key
 *
 * @fyn: The mapping node
 * @fyn_key: The node to use as key
 *
 * Returns:
 * The node pair matching the given key, or NULL if not found.
 */
struct fy_node_pair *fy_node_mapping_lookup_pair(struct fy_node *fyn, struct fy_node *fyn_key);

/**
 * fy_node_mapping_get_pair_index() - Return the node pair index in the mapping
 *
 * This method will return the node pair index in the mapping of the given
 * node pair argument.
 *
 * @fyn: The mapping node
 * @fynp: The node pair
 *
 * Returns:
 * The index of the node pair in the mapping or -1 in case of an error.
 */
int fy_node_mapping_get_pair_index(struct fy_node *fyn, const struct fy_node_pair *fynp);

/**
 * fy_node_pair_key() - Return the key of a node pair
 *
 * This method will return the node pair's key.
 * Note that this may be NULL, which is returned also in case
 * the node pair argument is NULL, so you should protect against
 * such a case.
 *
 * @fynp: The node pair
 *
 * Returns:
 * The node pair key
 */
struct fy_node *fy_node_pair_key(struct fy_node_pair *fynp);

/**
 * fy_node_pair_value() - Return the value of a node pair
 *
 * This method will return the node pair's value.
 * Note that this may be NULL, which is returned also in case
 * the node pair argument is NULL, so you should protect against
 * such a case.
 *
 * @fynp: The node pair
 *
 * Returns:
 * The node pair value
 */
struct fy_node *fy_node_pair_value(struct fy_node_pair *fynp);

/**
 * fy_node_pair_set_key() - Sets the key of a node pair
 *
 * This method will set the key part of the node pair.
 * It will ovewrite any previous key.
 *
 * Note that no checks for duplicate keys are going to be
 * performed.
 *
 * @fynp: The node pair
 * @fyn: The key node
 */
void fy_node_pair_set_key(struct fy_node_pair *fynp, struct fy_node *fyn);

/**
 * fy_node_pair_set_value() - Sets the value of a node pair
 *
 * This method will set the value part of the node pair.
 * It will ovewrite any previous value.
 *
 * @fynp: The node pair
 * @fyn: The value node
 */
void fy_node_pair_set_value(struct fy_node_pair *fynp, struct fy_node *fyn);

/**
 * fy_node_mapping_append() - Append a node item to a mapping
 *
 * Append a node pair to a mapping.
 *
 * @fyn_map: The mapping node
 * @fyn_key: The node pair's key
 * @fyn_value: The node pair's value
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_mapping_append(struct fy_node *fyn_map,
			   struct fy_node *fyn_key, struct fy_node *fyn_value);

/**
 * fy_node_mapping_prepend() - Prepend a node item to a mapping
 *
 * Prepend a node pair to a mapping.
 *
 * @fyn_map: The mapping node
 * @fyn_key: The node pair's key
 * @fyn_value: The node pair's value
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_mapping_prepend(struct fy_node *fyn_map,
			   struct fy_node *fyn_key, struct fy_node *fyn_value);

/**
 * fy_node_mapping_remove() - Remove a node pair from a mapping
 *
 * Remove node pair from a mapping.
 *
 * @fyn_map: The mapping node
 * @fynp: The node pair to remove
 *
 * Returns:
 * 0 on success, -1 on failure.
 */
int fy_node_mapping_remove(struct fy_node *fyn_map, struct fy_node_pair *fynp);

/**
 * fy_node_mapping_remove_by_key() - Remove a node pair from a mapping returning the value
 *
 * Remove node pair from a mapping using the supplied key.
 *
 * @fyn_map: The mapping node
 * @fyn_key: The node pair's key
 *
 * Returns:
 * The value part of removed node pair, or NULL in case of an error.
 */
struct fy_node *fy_node_mapping_remove_by_key(struct fy_node *fyn_map, struct fy_node *fyn_key);

/**
 * typedef fy_node_mapping_sort_fn - Mapping sorting method function
 *
 * @fynp_a: The first node_pair used in the comparison
 * @fynp_b: The second node_pair used in the comparison
 * @arg: The opaque user provided pointer to the sort operation
 *
 * Returns:
 * <0 if @fynp_a is less than @fynp_b
 * 0 if @fynp_a is equal to fynp_b
 * >0 if @fynp_a is greater than @fynp_b
 */
typedef int (*fy_node_mapping_sort_fn)(const struct fy_node_pair *fynp_a,
				       const struct fy_node_pair *fynp_b,
				       void *arg);

/**
 * fy_node_sort() - Recursively sort node
 *
 * Recursively sort all mappings of the given node, using the given
 * comparison method (if NULL use the default one).
 *
 * @fyn: The node to sort
 * @key_cmp: The comparison method
 * @arg: An opaque user pointer for the comparison method
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_node_sort(struct fy_node *fyn, fy_node_mapping_sort_fn key_cmp, void *arg);

/**
 * fy_node_vscanf() - Retrieve data via vscanf
 *
 * This method easily retrieves data using a familiar vscanf interface.
 * The format string is a regular scanf format string with the following format.
 *
 *   "pathspec %opt pathspec %opt..."
 *
 * Each pathspec is separated with space from the scanf option
 *
 * For example:
 * fyn = { foo: 3 } -> fy_node_scanf(fyn, "/foo %d", &var) -> var = 3
 *
 *
 * @fyn: The node to use as a pathspec root
 * @fmt: The scanf based format string
 * @ap: The va_list containing the arguments
 *
 * Returns:
 * The number of scanned arguments, or -1 on error.
 */
int fy_node_vscanf(struct fy_node *fyn, const char *fmt, va_list ap);

/**
 * fy_node_scanf() - Retrieve data via scanf
 *
 * This method easily retrieves data using a familiar vscanf interface.
 * The format string is a regular scanf format string with the following format.
 *
 *   "pathspec %opt pathspec %opt..."
 *
 * Each pathspec is separated with space from the scanf option
 *
 * For example:
 * fyn = { foo: 3 } -> fy_node_scanf(fyn, "/foo %d", &var) -> var = 3
 *
 *
 * @fyn: The node to use as a pathspec root
 * @fmt: The scanf based format string
 * @...: The arguments
 *
 * Returns:
 * The number of scanned arguments, or -1 on error.
 */
int fy_node_scanf(struct fy_node *fyn, const char *fmt, ...)
		__attribute__((format(scanf, 2, 3)));

/**
 * fy_document_vscanf() - Retrieve data via vscanf relative to document root
 *
 * This method easily retrieves data using a familiar vscanf interface.
 * The format string is a regular scanf format string with the following format.
 *
 *   "pathspec %opt pathspec %opt..."
 *
 * Each pathspec is separated with space from the scanf option
 *
 * For example:
 * fyd = { foo: 3 } -> fy_document_scanf(fyd, "/foo %d", &var) -> var = 3
 *
 *
 * @fyd: The document
 * @fmt: The scanf based format string
 * @ap: The va_list containing the arguments
 *
 * Returns:
 * The number of scanned arguments, or -1 on error.
 */
int fy_document_vscanf(struct fy_document *fyd, const char *fmt, va_list ap);

/**
 * fy_document_scanf() - Retrieve data via scanf relative to document root
 *
 * This method easily retrieves data using a familiar vscanf interface.
 * The format string is a regular scanf format string with the following format.
 *
 *   "pathspec %opt pathspec %opt..."
 *
 * Each pathspec is separated with space from the scanf option
 *
 * For example:
 * fyn = { foo: 3 } -> fy_node_scanf(fyd, "/foo %d", &var) -> var = 3
 *
 *
 * @fyd: The document
 * @fmt: The scanf based format string
 * @...: The arguments
 *
 * Returns:
 * The number of scanned arguments, or -1 on error.
 */
int fy_document_scanf(struct fy_document *fyd, const char *fmt, ...)
		__attribute__((format(scanf, 2, 3)));

/**
 * fy_document_tag_directive_iterate() - Iterate over a document's tag directives
 *
 * This method iterates over all the documents tag directives.
 * The start of the iteration is signalled by a NULL in \*prevp.
 *
 * @fyd: The document
 * @prevp: The previous state of the iterator
 *
 * Returns:
 * The next tag directive token in the document or NULL at the end of them.
 */
struct fy_token *fy_document_tag_directive_iterate(struct fy_document *fyd, void **prevp);

/**
 * fy_document_tag_directive_lookup() - Retreive a document's tag directive
 *
 * Retreives the matching tag directive token of the document matching the handle.
 *
 * @fyd: The document
 * @handle: The handle to look for
 *
 * Returns:
 * The tag directive token with the given handle or NULL if not found
 */
struct fy_token *fy_document_tag_directive_lookup(struct fy_document *fyd, const char *handle);

/**
 * fy_document_tag_directive_add() - Add a tag directive to a document
 *
 * Add tag directive to the document.
 *
 * @fyd: The document
 * @handle: The handle of the tag directive
 * @prefix: The prefix of the tag directive
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_document_tag_directive_add(struct fy_document *fyd, const char *handle, const char *prefix);

/**
 * fy_document_tag_directive_remove() - Remove a tag directive
 *
 * Remove a tag directive from a document.
 * Note that removal is prohibited if any node is still using this tag directive.
 *
 * @fyd: The document
 * @handle: The handle of the tag directive to remove.
 *
 * Returns:
 * 0 on success, -1 on error
 */
int fy_document_tag_directive_remove(struct fy_document *fyd, const char *handle);

/**
 * fy_document_lookup_anchor() - Lookup an anchor
 *
 * Lookup for an anchor having the name provided
 * 
 * @fyd: The document
 * @anchor: The anchor to look for
 *
 * Returns:
 * The anchor if found, NULL otherwise
 */
struct fy_anchor *fy_document_lookup_anchor(struct fy_document *fyd, const char *anchor);

/**
 * fy_document_lookup_anchor_by_token() - Lookup an anchor by token text
 *
 * Lookup for an anchor having the name provided from the text of the token
 *
 * @fyd: The document
 * @anchor: The token contains the anchor text to look for
 *
 * Returns:
 * The anchor if found, NULL otherwise
 */
struct fy_anchor *fy_document_lookup_anchor_by_token(struct fy_document *fyd, struct fy_token *anchor);

/**
 * fy_document_lookup_anchor_by_node() - Lookup an anchor by node
 *
 * Lookup for an anchor located in the given node
 *
 * @fyd: The document
 * @fyn: The node
 *
 * Returns:
 * The anchor if found, NULL otherwise
 */
struct fy_anchor *fy_document_lookup_anchor_by_node(struct fy_document *fyd, struct fy_node *fyn);

/**
 * fy_anchor_get_text() - Get the text of an anchor
 *
 * This method will return a pointer to the text of an anchor
 * along with the length of it. Note that this text is *not*
 * NULL terminated.
 *
 * @fya: The anchor
 * @lenp: Pointer to a variable that will hold the returned length
 *
 * Returns:
 * A pointer to the text of the anchor, while @lenp will be assigned the
 * length of said anchor.
 * A NULL will be returned in case of an error.
 */
const char *fy_anchor_get_text(struct fy_anchor *fya, size_t *lenp);

/**
 * fy_anchor_node() - Get the node of an anchor
 *
 * This method returns the node associated with the anchor.
 *
 * @fya: The anchor
 *
 * Returns:
 * The node of the anchor, or NULL in case of an error.
 */
struct fy_node *fy_anchor_node(struct fy_anchor *fya);

/**
 * fy_document_anchor_iterate() - Iterate over a document's anchors
 *
 * This method iterates over all the documents anchors.
 * The start of the iteration is signalled by a NULL in \*prevp.
 *
 * @fyd: The document
 * @prevp: The previous state of the iterator
 *
 * Returns:
 * The next anchor in the document or NULL at the end of them.
 */
struct fy_anchor *fy_document_anchor_iterate(struct fy_document *fyd, void **prevp);

/**
 * fy_document_set_anchor() - Place an anchor
 *
 * Places an anchor to the node with the give text name.
 *
 * Note that the data are not copied, merely a reference is taken, so
 * it must be available while the node is in use.
 *
 * @fyd: The document
 * @fyn: The node to set the anchor to
 * @text: Pointer to the anchor text
 * @len: Size of the anchor text, or (size_t)-1 for '\0' terminated.
 *
 * Returns:
 * 0 on success, -1 on error.
 */
int fy_document_set_anchor(struct fy_document *fyd, struct fy_node *fyn, const char *text, size_t len);

/**
 * fy_node_set_anchor() - Place an anchor to the node's document
 *
 * Places an anchor to the node with the give text name.
 *
 * Note that the data are not copied, merely a reference is taken, so
 * it must be available while the node is in use.
 *
 * This is similar to fy_document_set_anchor() with the document set
 * to the document of the @fyn node.
 *
 * @fyn: The node to set the anchor to
 * @text: Pointer to the anchor text
 * @len: Size of the anchor text, or (size_t)-1 for '\0' terminated.
 *
 * Returns:
 * 0 on success, -1 on error.
 */
int fy_node_set_anchor(struct fy_node *fyn, const char *text, size_t len);

/**
 * fy_node_remove_anchor() - Remove an anchor
 *
 * Remove an anchor for the given node (if it exists)
 *
 * @fyn: The node to remove anchors from
 *
 * Returns:
 * 0 on success, -1 on error.
 */
int fy_node_remove_anchor(struct fy_node *fyn);

/**
 * fy_node_get_anchor() - Get the anchor of a node
 *
 * Retrieve the anchor of the given node (if it exists)
 *
 * @fyn: The node
 *
 * Returns:
 * The anchor if there's one at the node, or NULL otherwise
 */
struct fy_anchor *fy_node_get_anchor(struct fy_node *fyn);

/**
 * fy_node_create_alias() - Create an alias node
 *
 * Create an alias on the given document
 *
 * @fyd: The document
 * @alias: The alias text
 *
 * Returns:
 * The created alias node, or NULL in case of an error
 */
struct fy_node *fy_node_create_alias(struct fy_document *fyd, const char *alias);

#endif
