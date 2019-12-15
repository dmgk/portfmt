/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Tobias Kortkamp <tobik@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <regex.h>
#include <stdlib.h>
#include <stdio.h>

#include "array.h"
#include "conditional.h"
#include "parser.h"
#include "parser/plugin.h"
#include "token.h"
#include "util.h"
#include "variable.h"

static void
add_clones(struct Array *clones, struct Array *seen, struct Array *seen_in_cond)
{
	for (size_t i = 0; i < array_len(seen_in_cond); i++) {
		char *name = array_get(seen_in_cond, i);
		if (array_find(seen, name, str_compare, NULL) > -1 &&
		    array_find(clones, name, str_compare, NULL) == -1) {
			array_append(clones, xstrdup(name));
		}
	}
	array_truncate(seen_in_cond);
}

static struct Array *
lint_clones(struct Parser *parser, struct Array *ptokens, enum ParserError *error, char **error_msg, const void *userdata)
{
	struct Array **clones_ret = (struct Array **)userdata;
	int no_color = parser_settings(parser).behavior & PARSER_OUTPUT_NO_COLOR;

	struct Array *seen = array_new();
	struct Array *seen_in_cond = array_new();
	struct Array *clones = array_new();
	int in_conditional = 0;
	for (size_t i = 0; i < array_len(ptokens); i++) {
		struct Token *t = array_get(ptokens, i);
		switch (token_type(t)) {
		case CONDITIONAL_START:
			switch(conditional_type(token_conditional(t))) {
			case COND_FOR:
			case COND_IF:
			case COND_IFDEF:
			case COND_IFNDEF:
			case COND_IFMAKE:
				in_conditional++;
				break;
			case COND_ENDFOR:
			case COND_ENDIF:
				in_conditional--;
				if (in_conditional <= 0) {
					add_clones(clones, seen, seen_in_cond);
				}
				break;
			default:
				break;
			}
			break;
		case VARIABLE_START: {
			struct Variable *v = token_variable(t);
			if (variable_modifier(v) == MODIFIER_ASSIGN) {
				char *name = variable_name(v);
				if (in_conditional > 0) {
					if (array_find(seen_in_cond, name, str_compare, NULL) == -1) {
						array_append(seen_in_cond, name);
					}
				} else {
					if (array_find(seen, name, str_compare, NULL) == -1) {
						array_append(seen, name);
					} else if (array_find(clones, name, str_compare, NULL) == -1) {
						array_append(clones, name);
					}
				}
			}
			break;
		} default:
			break;
		}
	}

	array_sort(clones, str_compare, NULL);

	if (clones_ret == NULL && array_len(clones) > 0) {
		if (!no_color) {
			parser_enqueue_output(parser, ANSI_COLOR_CYAN);
		}
		parser_enqueue_output(parser, "# Variables set twice or more\n");
		if (!no_color) {
			parser_enqueue_output(parser, ANSI_COLOR_RESET);
		}
		for (size_t i = 0; i < array_len(clones); i++) {
			char *name = array_get(clones, i);
			parser_enqueue_output(parser, name);
			parser_enqueue_output(parser, "\n");
		}
	}

	array_free(seen);
	array_free(seen_in_cond);
	if (clones_ret == NULL) {
		array_free(clones);
	} else {
		*clones_ret = clones;
	}

	return NULL;
}

PLUGIN("lint.clones", lint_clones);
