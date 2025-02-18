libfyaml
&middot;

====

A fancy 1.3 YAML and JSON parser/writer.

Fully feature complete YAML parser and emitter, supporting the latest YAML spec and
passing the full YAML testsuite.

It is designed to be very efficient, avoiding copies of data, and
has no artificial limits like the 1024 character limit for implicit keys.

## Features
* Fully supports YAML version 1.3.
* Zero content copy operation, which means that content is never copied to
  internal structures. On input types that support it (mmap files and
  constant strings) that means that memory usage is kept low, and arbitrary
  large content can be manipulated without problem.
* Parser may be used in event mode (like libyaml) or in document generating mode.
* Extensive programmable API capable of manipulating parsed YAML documents or
  creating them from scratch.
* YAML emitter with programmable options, supporting colored output.
* Extensive testsuite for the API, the full YAML test-suite and correct
  emitter operation.
* Easy printf/scanf based YAML creation and data extraction API.
* Accurate and descriptive error messages, in standard compiler format that 
  can be parsed by editors and developer GUIs.
* Testsuite supports running under valgrind and checking for memory leaks. No
  leaks should be possible under normal operation, so it is usable for long-
  running applications.

## Contents
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Usage and examples](#usage-and-examples)
- [API documentation](#api-documentation)
- [fy-tool reference](#fy-tool-reference)
- [fy-dump reference](#fy-dump-reference)
- [fy-testsuite reference](#fy-testsuite-reference)
- [fy-filter reference](#fy-filter-reference)
- [fy-join reference](#fy-join-reference)
- [Missing Features](#missing-features)

## Prerequisites

libfyaml is primarily developed on Linux based debian distros but Apple MacOS X builds
(using homebrew) are supported as well.

On a based debian distro (i.e. ubuntu 19.04 disco) you should install the following
dependencies:

* `sudo apt-get install gcc autoconf automake libtool git make libltdl-dev pkg-config`

To enable the libyaml comparison checker:

* `sudo apt-get install libyaml-dev`

For the API testsuite libcheck is required:

* `sudo apt-get install check`

And finally in order to build the sphinx based documentation:

* `sudo apt-get install python3 python3-pip python3-setuptools`
* `pip3 install wheel sphinx git+http://github.com/return42/linuxdoc.git sphinx\_rtd\_theme sphinx-markdown-builder`

Note that some older distros (like xenial) do not have a sufficiently recent
sphinx in their repos. In that case you can create a virtual environment
using scripts/create-virtual-env

## Building

``libfyaml`` uses a standard autotools based build scheme so:

* `./bootstrap.sh`
* `./configure`
* `make`

Will build the library and `fy-tool`.

* `make check`

Will run the test-suite.

Binaries, libraries, header files and pkgconfig files maybe installed with

* `make install`

By default, the installation prefix will be `/usr/local`, which you can change
with the `--prefix <dir>` option during configure.

To build the documentation API in HTML format use:

* `make doc-html`

The documentation for the public API will be found in doc/\_build/html

* `make doc-latexpdf`

Will generate a single pdf containing everything.

## Usage and examples

Usage of libfyaml is somewhat similar to libyaml, but with a few notable differences.

1. The objects of the library are opaque, they are pointers that may be used but
   may not be derefenced via library users. This makes the public API not be dependent of
   internal changes in the library structures.

2. The object pointers used are guaranteed to not 'move' like libyaml object pointers
   so you may embed them freely in your own structures.

3. The convenience methods of libyaml allow you to avoid tedious iteration and code
   duplication. While fully manual YAML document tree manipulation is available, if your
   application is not performance sensitive when manipulating YAML, you are advised to
   use the helpers.

### Using libfyaml in your projects

Typically you only have to include the single header file `libfyaml.h` and
link against the correct fyaml-\<major\>-\<minor\> library.

It is recommended to use pkg-config, i.e.

```make
CFLAGS+= `pkg-config --cflags libfyaml`
LDFLAGS+= `pkg-config --libs libfyaml`
```

For use in automake based project you may use the following fragment
```bash
PKG_CHECK_MODULES(LIBFYAML, [ libfyaml ], HAVE_LIBFYAML=1, HAVE_LIBFYAML=0)

if test "x$HAVE_LIBFYAML" != "x1" ; then
	AC_MSG_ERROR([failed to find libfyaml])
fi

AC_SUBST(HAVE_LIBFYAML)
AC_SUBST(LIBFYAML_CFLAGS)
AC_SUBST(LIBFYAML_LIBS)
AC_DEFINE_UNQUOTED([HAVE_LIBFYAML], [$HAVE_LIBFYAML], [Define to 1 if you have libfyaml available])
AM_CONDITIONAL([HAVE_LIBFYAML], [ test x$HAVE_LIBFYAML = x1 ])
```

The examples that follow will make things clear.

### Display libfyaml version example

This is the minimal example that checks that you've compiled against the correct libfyaml.

```c
/*
 * fy-version.c - libfyaml version example
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include <libfyaml.h>

int main(int argc, char *argv[])
{
	printf("%s\n", fy_library_version());
	return EXIT_SUCCESS;
}
```

### libfyaml example using simplified inprogram YAML generation

This example simply parses an in-program YAML string and displays
a string.

The standard header plus variables definition.

```c
/*
 * inprogram.c - libfyaml inprogram YAML example
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <libfyaml.h>

int main(int argc, char *argv[])
{
	static const char *yaml = 
		"invoice: 34843\n"
		"date   : !!str 2001-01-23\n"
		"bill-to: &id001\n"
		"    given  : Chris\n"
		"    family : Dumars\n"
		"    address:\n"
		"        lines: |\n"
		"            458 Walkman Dr.\n"
		"            Suite #292\n";
	struct fy_document *fyd = NULL;
	int rc, count, ret = EXIT_FAILURE;
	unsigned int invoice_nr;
	char given[256 + 1];
```

Parsing and creating a YAML document from either the built-in
YAML, or an invoice file given on the command line:

```c
	if (argc == 1)
		fyd = fy_document_build_from_string(NULL, yaml);
	else
		fyd = fy_document_build_from_file(NULL, argv[1]);
	if (!fyd) {
		fprintf(stderr, "failed to build document");
		goto fail;
	}
```

Get the invoice number and the given name using a single call.

```c
	/* get the invoice number and the given name */
	count = fy_document_scanf(fyd,
			"/invoice %u "
			"/bill-to/given %256s",
			&invoice_nr, given);
	if (count != 2) {
		fprintf(stderr, "Failed to retreive the two items\n");
		goto fail;
	}

	/* print them as comments in the emitted YAML */
	printf("# invoice number was %u\n", invoice_nr);
	printf("# given name is %s\n", given);
```

In sequence, increase the invoice number, add a spouce and a secondary
address.

```c
	rc =
		/* set increased invoice number (modify existing node) */
		fy_document_insert_at(fyd, "/invoice",
			fy_node_buildf(fyd, "%u", invoice_nr + 1)) ||
		/* add spouce (create new mapping pair) */
		fy_document_insert_at(fyd, "/bill-to",
			fy_node_buildf(fyd, "spouse: %s", "Doris")) ||
		/* add a second address */
		fy_document_insert_at(fyd, "/bill-to",
			fy_node_buildf(fyd, "delivery-address:\n"
				            "  lines: |\n"
					    "    1226 Windward Ave.\n"));
	if (rc) {
		fprintf(stderr, "failed to insert to document\n");
		goto fail;
	}
```

Emit the document to standard output (while sorting the keys)

```c
	/* emit the document to stdout (but sorted) */
	rc = fy_emit_document_to_fp(fyd, FYECF_DEFAULT | FYECF_SORT_KEYS, stdout);
	if (rc) {
		fprintf(stderr, "failed to emit document to stdout");
		goto fail;
	}
```

Finally exit and report condition.

```c
	ret = EXIT_SUCCESS;
fail:
	fy_document_destroy(fyd);	/* NULL is OK */

	return ret;
}
```

## API documentation

For complete documentation of libfyaml API, visit https://pantoniou.github.io/libfyaml/

## Missing features and omissions

1. Windows - libfyaml is not supporting windows yet.
2. Unicode - libfyaml only supports UTF8 and has no support for wide character input.

## Development and contributing
Feel free to send pull requests and raise issues.
