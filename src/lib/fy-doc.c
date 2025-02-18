/*
 * fy-doc.c - YAML document methods
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * SPDX-License-Identifier: MIT
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <libfyaml.h>

#include "fy-parse.h"
#include "fy-doc.h"

#include "fy-utils.h"

struct fy_document_state *fy_document_state_alloc(void)
{
	struct fy_document_state *fyds;

	fyds = malloc(sizeof(*fyds));
	if (!fyds)
		return NULL;
	memset(fyds, 0, sizeof(*fyds));

	fyds->fyt_vd = NULL;
	fy_token_list_init(&fyds->fyt_td);

	fyds->refs = 1;

	/* fy_notice(NULL, "%s: %p #%d", __func__, fyds, fyds->refs); */

	return fyds;
}

void fy_document_state_free(struct fy_document_state *fyds)
{
	if (!fyds)
		return;

	assert(fyds->refs == 1);

	/* fy_notice(NULL, "%s: %p #%d", __func__, fyds, fyds->refs); */

	fy_token_unref(fyds->fyt_vd);
	fy_token_list_unref_all(&fyds->fyt_td);

	free(fyds);
}

struct fy_document_state *fy_document_state_ref(struct fy_document_state *fyds)
{
	if (!fyds)
		return NULL;


	assert(fyds->refs + 1 > 0);

	fyds->refs++;

	/* fy_notice(NULL, "%s: %p #%d", __func__, fyds, fyds->refs); */

	return fyds;
}

void fy_document_state_unref(struct fy_document_state *fyds)
{
	if (!fyds)
		return;

	assert(fyds->refs > 0);

	/* fy_notice(NULL, "%s: %p #%d", __func__, fyds, fyds->refs); */

	if (fyds->refs == 1)
		fy_document_state_free(fyds);
	else
		fyds->refs--;
}

struct fy_document_state *fy_parse_document_state_alloc(struct fy_parser *fyp)
{
	struct fy_document_state *fyds;

	if (!fyp)
		return NULL;

	fyds = fy_document_state_alloc();
	if (!fyds)
		return NULL;

	return fyds;
}

void fy_parse_document_state_recycle(struct fy_parser *fyp, struct fy_document_state *fyds)
{
	fy_document_state_unref(fyds);
}

static void fy_resolve_parent_node(struct fy_document *fyd, struct fy_node *fyn, struct fy_node *fyn_parent);
int fy_document_state_merge(struct fy_document *fyd, struct fy_document *fydc);

void fy_anchor_destroy(struct fy_anchor *fya)
{
	if (!fya)
		return;
	fy_token_unref(fya->anchor);
	free(fya);
}

struct fy_anchor *fy_anchor_create(struct fy_document *fyd,
		struct fy_node *fyn, struct fy_token *anchor)
{
	struct fy_anchor *fya = NULL;

	fya = malloc(sizeof(*fya));
	fy_error_check(fyd->fyp, fya, err_out,
			"malloc() failed");

	fya->fyn = fyn;
	fya->anchor = anchor;

	return fya;

err_out:
	fy_anchor_destroy(fya);
	return NULL;
}

struct fy_anchor *fy_document_anchor_iterate(struct fy_document *fyd, void **prevp)
{
	struct fy_anchor_list *fyal;

	if (!fyd || !prevp)
		return NULL;

	fyal = &fyd->anchors;

	return *prevp = *prevp ? fy_anchor_next(fyal, *prevp) : fy_anchor_list_head(fyal);
}

int fy_document_set_anchor(struct fy_document *fyd, struct fy_node *fyn, const char *text, size_t len)
{
	struct fy_anchor *fya;
	struct fy_input *fyi = NULL;
	struct fy_token *fyt = NULL;
	struct fy_atom handle;

	if (!fyd)
		return -1;

	if (text && len == (size_t)-1)
		len = strlen(text);

	/* XXX only destroy if no references to this anchor exist */
	fya = fy_document_lookup_anchor_by_node(fyd, fyn);
	if (fya) {
		fy_anchor_list_del(&fyd->anchors, fya);
		fy_anchor_destroy(fya);
	}
	fya = NULL;

	if (!text)
		return 0;

	fyi = fy_parse_input_from_data(fyd->fyp, text, len, &handle, true);
	if (!fyi)
		goto err_out;

	fyt = fy_token_create(fyd->fyp, FYTT_ANCHOR, &handle);
	if (!fyt)
		goto err_out;

	fya = fy_anchor_create(fyd, fyn, fyt);
	if (!fya)
		goto err_out;

	fy_anchor_list_add(&fyd->anchors, fya);

	return 0;
err_out:
	fy_anchor_destroy(fya);
	fy_token_unref(fyt);
	fy_input_unref(fyi);
	return -1;
}

int fy_node_set_anchor(struct fy_node *fyn, const char *text, size_t len)
{
	if (!fyn)
		return -1;
	return fy_document_set_anchor(fyn->fyd, fyn, text, len);
}

int fy_node_remove_anchor(struct fy_node *fyn)
{
	return fy_node_set_anchor(fyn, NULL, 0);
}

struct fy_anchor *fy_node_get_anchor(struct fy_node *fyn)
{
	if (!fyn)
		return NULL;

	return fy_document_lookup_anchor_by_node(fyn->fyd, fyn);
}

void fy_parse_document_destroy(struct fy_parser *fyp, struct fy_document *fyd)
{
	struct fy_anchor *fya;

	if (!fyp || !fyd)
		return;

	if (fyd->errfp)
		fclose(fyd->errfp);

	if (fyd->errbuf)
		free(fyd->errbuf);

	fy_node_free(fyd->root);

	/* remove all anchors */
	while ((fya = fy_anchor_list_pop(&fyd->anchors)) != NULL)
		fy_anchor_destroy(fya);

	fy_document_state_unref(fyd->fyds);

	/* and release all the remaining tracked memory */
	fy_tfree_all(&fyd->tallocs);

	fy_parse_free(fyp, fyd);
}

struct fy_token *fy_document_state_lookup_tag_directive(struct fy_document_state *fyds,
		const char *handle, size_t handle_size)
{
	const char *td_handle;
	size_t td_handle_size;
	struct fy_token *fyt;

	if (!fyds)
		return NULL;

	for (fyt = fy_token_list_first(&fyds->fyt_td); fyt; fyt = fy_token_next(&fyds->fyt_td, fyt)) {

		td_handle = fy_tag_directive_token_handle(fyt, &td_handle_size);
		assert(td_handle);

		if (handle_size == td_handle_size && !memcmp(handle, td_handle, handle_size))
			return fyt;

	}

	return NULL;
}

struct fy_document *fy_parse_document_create(struct fy_parser *fyp, struct fy_eventp *fyep)
{
	struct fy_document *fyd = NULL;
	struct fy_document_state *fyds;
	struct fy_event *fye = NULL;
	struct fy_error_ctx ec;

	if (!fyp || !fyep)
		return NULL;

	fye = &fyep->e;

	FY_ERROR_CHECK(fyp, fy_document_event_get_token(fye), &ec, FYEM_DOC,
			fye->type == FYET_DOCUMENT_START, err_stream_start);

	fyd = fy_parse_alloc(fyp, sizeof(*fyd));
	fy_error_check(fyp, fyd, err_out,
		"fy_parse_alloc() failed");

	memset(fyd, 0, sizeof(*fyd));

	fyd->fyp = fyp;
	fy_talloc_list_init(&fyd->tallocs);

	fy_anchor_list_init(&fyd->anchors);
	fyd->root = NULL;

	fyds = fye->document_start.document_state;
	fye->document_start.document_state = NULL;

	/* and we're done with this event */
	fy_parse_eventp_recycle(fyp, fyep);

	/* drop the old reference */
	fy_document_state_unref(fyd->fyds);

	/* note that we keep the reference */
	fyd->fyds = fyds;

	fyd->errfp = NULL;
	fyd->errbuf = NULL;
	fyd->errsz = 0;

	fy_document_list_init(&fyd->children);

	return fyd;

err_out:
	fy_parse_document_destroy(fyp, fyd);
	fy_parse_eventp_recycle(fyp, fyep);
	return NULL;

err_stream_start:
	fy_error_report(fyp, &ec, "invalid start of event stream");
	goto err_out;
}

struct fy_anchor *fy_document_lookup_anchor(struct fy_document *fyd, const char *anchor)
{
	struct fy_anchor *fya;
	struct fy_anchor_list *fyal;
	const char *text;
	size_t len, text_len;

	if (!fyd || !anchor)
		return NULL;

	len = strlen(anchor);

	fyal = &fyd->anchors;
	for (fya = fy_anchor_list_head(fyal); fya; fya = fy_anchor_next(fyal, fya)) {
		text = fy_anchor_get_text(fya, &text_len);
		if (!text)
			return NULL;

		if (len == text_len && !memcmp(anchor, text, len))
			return fya;
	}

	return NULL;
}

struct fy_anchor *fy_document_lookup_anchor_by_token(struct fy_document *fyd, struct fy_token *anchor)
{
	const char *text;
	size_t len;
	char *text0;

	if (!fyd || !anchor)
		return NULL;

	text = fy_token_get_text(anchor, &len);
	if (!text)
		return NULL;
	text0 = alloca(len + 1);
	memcpy(text0, text, len);
	text0[len] = '\0';

	return fy_document_lookup_anchor(fyd, text0);
}

struct fy_anchor *fy_document_lookup_anchor_by_node(struct fy_document *fyd, struct fy_node *fyn)
{
	struct fy_anchor *fya;
	struct fy_anchor_list *fyal;

	if (!fyd || !fyn)
		return NULL;

	fyal = &fyd->anchors;
	for (fya = fy_anchor_list_head(fyal); fya; fya = fy_anchor_next(fyal, fya)) {
		if (fya->fyn == fyn)
			return fya;
	}

	return NULL;
}

const char *fy_anchor_get_text(struct fy_anchor *fya, size_t *lenp)
{
	if (!fya || !lenp)
		return NULL;
	return fy_token_get_text(fya->anchor, lenp);
}

struct fy_node *fy_anchor_node(struct fy_anchor *fya)
{
	if (!fya)
		return NULL;
	return fya->fyn;
}

void fy_node_pair_free(struct fy_node_pair *fynp)
{
	if (!fynp)
		return;

	fy_node_free(fynp->key);
	fy_node_free(fynp->value);

	free(fynp);
}

struct fy_node_pair *fy_node_pair_alloc(struct fy_document *fyd)
{
	struct fy_parser *fyp = fyd->fyp;
	struct fy_node_pair *fynp = NULL;

	fynp = malloc(sizeof(*fynp));
	fy_error_check(fyp, fynp, err_out,
			"malloc() failed");

	fynp->key = NULL;
	fynp->value = NULL;
	fynp->fyd = fyd;
	return fynp;

err_out:
	return NULL;
}

void fy_node_free(struct fy_node *fyn)
{
	struct fy_document *fyd;
	struct fy_node *fyni;
	struct fy_node_pair *fynp;
	struct fy_anchor *fya, *fyan;

	if (!fyn)
		return;

	fyd = fyn->fyd;
	assert(fyd);

	/* remove anchors that are located on this node */
	for (fya = fy_anchor_list_head(&fyd->anchors); fya; fya = fyan) {
		fyan = fy_anchor_next(&fyd->anchors, fya);
		if (fya->fyn == fyn) {
			fy_anchor_list_del(&fyd->anchors, fya);
			fy_anchor_destroy(fya);
		}
	}


	fy_token_unref(fyn->tag);
	fyn->tag = NULL;
	switch (fyn->type) {
	case FYNT_SCALAR:
		fy_token_unref(fyn->scalar);
		fyn->scalar = NULL;
		break;
	case FYNT_SEQUENCE:
		while ((fyni = fy_node_list_pop(&fyn->sequence)) != NULL)
			fy_node_free(fyni);
		fy_token_unref(fyn->sequence_start);
		fy_token_unref(fyn->sequence_end);
		break;
	case FYNT_MAPPING:
		while ((fynp = fy_node_pair_list_pop(&fyn->mapping)) != NULL)
			fy_node_pair_free(fynp);
		fy_token_unref(fyn->mapping_start);
		fy_token_unref(fyn->mapping_end);
		break;
	}

	free(fyn);
}

struct fy_node *fy_node_alloc(struct fy_document *fyd, enum fy_node_type type)
{
	struct fy_parser *fyp = fyd->fyp;
	struct fy_node *fyn = NULL;

	fyn = malloc(sizeof(*fyn));
	fy_error_check(fyp, fyn, err_out,
			"malloc() failed");
	memset(fyn, 0, sizeof(*fyn));
	fyn->type = type;
	fyn->style = FYNS_ANY;
	fyn->fyd = fyd;

	switch (fyn->type) {
	case FYNT_SCALAR:
		fyn->scalar = NULL;
		break;
	case FYNT_SEQUENCE:
		fy_node_list_init(&fyn->sequence);
		break;
	case FYNT_MAPPING:
		fy_node_pair_list_init(&fyn->mapping);
		break;
	}
	return fyn;

err_out:
	fy_node_free(fyn);
	return NULL;
}

const struct fy_mark *fy_node_get_start_mark(struct fy_node *fyn)
{
	const struct fy_mark *fym = NULL;
	struct fy_node_pair *fynp;

	if (!fyn)
		return NULL;

	switch (fyn->type) {
	case FYNT_SCALAR:
		fym = fy_token_start_mark(fyn->scalar);
		break;

	case FYNT_SEQUENCE:
		fym = fy_token_start_mark(fyn->sequence_start);
		/* no explicit sequence start, use the start mark of the first item */
		if (!fym)
			fym = fy_node_get_start_mark(fy_node_list_head(&fyn->sequence));
		break;

	case FYNT_MAPPING:
		fym = fy_token_start_mark(fyn->mapping_start);
		/* no explicit mapping start, use the start mark of the first key */
		if (!fym) {
			fynp = fy_node_pair_list_head(&fyn->mapping);
			if (fynp)
				fym = fy_node_get_start_mark(fynp->key);
		}
		break;

	}

	assert(fym);

	return fym;
}

const struct fy_mark *fy_node_get_end_mark(struct fy_node *fyn)
{
	const struct fy_mark *fym = NULL;
	struct fy_node_pair *fynp;

	if (!fyn)
		return NULL;

	switch (fyn->type) {
	case FYNT_SCALAR:
		fym = fy_token_end_mark(fyn->scalar);
		break;

	case FYNT_SEQUENCE:
		fym = fy_token_end_mark(fyn->sequence_end);
		/* no explicit sequence end, use the end mark of the last item */
		if (!fym)
			fym = fy_node_get_end_mark(fy_node_list_tail(&fyn->sequence));
		break;

	case FYNT_MAPPING:
		fym = fy_token_end_mark(fyn->mapping_end);
		/* no explicit mapping end, use the end mark of the last value */
		if (!fym) {
			fynp = fy_node_pair_list_tail(&fyn->mapping);
			if (fynp)
				fym = fy_node_get_end_mark(fynp->value);
		}
		break;

	}

	assert(fym);

	return fym;
}

struct fy_input *fy_node_get_input(struct fy_node *fyn)
{
	struct fy_input *fyi = NULL;
	struct fy_node_pair *fynp;

	if (!fyn)
		return NULL;

	switch (fyn->type) {
	case FYNT_SCALAR:
		fyi = fy_token_get_input(fyn->scalar);
		break;

	case FYNT_SEQUENCE:
		fyi = fy_token_get_input(fyn->sequence_start);
		/* no explicit sequence start, use the start mark of the first item */
		if (!fyi)
			fyi = fy_node_get_input(fy_node_list_head(&fyn->sequence));
		break;

	case FYNT_MAPPING:
		fyi = fy_token_get_input(fyn->mapping_start);
		/* no explicit mapping start, use the start mark of the first key */
		if (!fyi) {
			fynp = fy_node_pair_list_head(&fyn->mapping);
			if (fynp)
				fyi = fy_node_get_input(fynp->key);
		}
		break;

	}

	assert(fyi);

	return fyi;
}

int fy_parse_document_register_anchor(struct fy_parser *fyp, struct fy_document *fyd,
				      struct fy_node *fyn, struct fy_token *anchor)
{
	struct fy_anchor *fyax, *fya = NULL;
	struct fy_token *anchor_new = NULL;
	struct fy_error_ctx ec;
	int rc;

	fyax = fy_document_lookup_anchor_by_token(fyd, anchor);

	FY_ERROR_CHECK(fyp, anchor, &ec, FYEM_DOC,
			!fyax, err_duplicate_anchor);

	anchor_new = fy_token_ref(anchor);
	fya = fy_anchor_create(fyd, fyn, anchor_new);
	fy_error_check(fyp, fya, err_out,
			"fy_anchor_create() failed");

	fy_anchor_list_add(&fyd->anchors, fya);

	return 0;

err_out:
	rc = -1;
	fy_token_unref(anchor_new);
	fy_anchor_destroy(fya);
	return rc;

err_duplicate_anchor:
	fy_error_report(fyp, &ec, "duplicate anchor");
	goto err_out;
}

bool fy_node_compare(struct fy_node *fyn1, struct fy_node *fyn2)
{
	struct fy_node *fyni1, *fyni2;
	struct fy_node_pair *fynp1, *fynp2;
	const char *t1, *t2;
	size_t l1, l2;
	bool ret, null1, null2;
	struct fy_node_pair **fynpp1, **fynpp2;
	int i, count1, count2;

	/* equal pointers? */
	if (fyn1 == fyn2)
		return true;

	null1 = !fyn1 || (fyn1->type == FYNT_SCALAR && fy_token_get_text_length(fyn1->scalar) == 0);
	null2 = !fyn2 || (fyn2->type == FYNT_SCALAR && fy_token_get_text_length(fyn2->scalar) == 0);

	/* both null */
	if (null1 && null2)
		return true;

	/* either is NULL, no match */
	if (null1 || null2)
		return false;

	/* types must match */
	if (fyn1->type != fyn2->type)
		return false;

	ret = true;

	switch (fyn1->type) {
	case FYNT_SEQUENCE:
		fyni1 = fy_node_list_head(&fyn1->sequence);
		fyni2 = fy_node_list_head(&fyn2->sequence);
		while (fyni1 && fyni2) {

			ret = fy_node_compare(fyni1, fyni2);
			if (!ret)
				break;

			fyni1 = fy_node_next(&fyn1->sequence, fyni1);
			fyni2 = fy_node_next(&fyn2->sequence, fyni2);
		}
		if (ret && fyni1 != fyni2 && (!fyni1 || !fyni2))
			ret = false;

		break;

	case FYNT_MAPPING:
		count1 = fy_node_mapping_item_count(fyn1);
		count2 = fy_node_mapping_item_count(fyn2);

		/* mapping counts must match */
		if (count1 != count2) {
			ret = false;
			break;
		}

		fynpp1 = alloca(sizeof(*fynpp1) * (count1 + 1));
		fynpp2 = alloca(sizeof(*fynpp2) * (count2 + 1));

		fy_node_mapping_perform_sort(fyn1, NULL, NULL, fynpp1, count1);
		fy_node_mapping_perform_sort(fyn2, NULL, NULL, fynpp2, count2);

		for (i = 0; i < count1; i++) {
			fynp1 = fynpp1[i];
			fynp2 = fynpp2[i];

			ret = fy_node_compare(fynp1->key, fynp2->key);
			if (!ret)
				break;

			ret = fy_node_compare(fynp1->value, fynp2->value);
			if (!ret)
				break;
		}
		if (i >= count1)
			ret = true;

		break;

	case FYNT_SCALAR:
		t1 = fy_token_get_text(fyn1->scalar, &l1);
		t2 = fy_token_get_text(fyn2->scalar, &l2);
		if (l1 != l2)
			return false;
		ret = !memcmp(t1, t2, l1);
		break;
	}

	return ret;
}

bool fy_node_compare_string(struct fy_node *fyn, const char *str)
{
	struct fy_document *fyd = NULL;
	bool ret;

	fyd = fy_document_build_from_string(NULL, str);
	if (!fyd)
		return false;

	ret = fy_node_compare(fyn, fy_document_root(fyd));

	fy_document_destroy(fyd);

	return ret;
}

struct fy_node_pair *fy_node_mapping_lookup_pair(struct fy_node *fyn, struct fy_node *fyn_key)
{
	struct fy_node_pair *fynpi;

	for (fynpi = fy_node_pair_list_head(&fyn->mapping); fynpi;
		fynpi = fy_node_pair_next(&fyn->mapping, fynpi)) {

		if (fy_node_compare(fynpi->key, fyn_key))
			return fynpi;
	}

	return NULL;
}

int fy_node_mapping_get_pair_index(struct fy_node *fyn, const struct fy_node_pair *fynp)
{
	struct fy_node_pair *fynpi;
	int i;

	for (i = 0, fynpi = fy_node_pair_list_head(&fyn->mapping); fynpi;
		fynpi = fy_node_pair_next(&fyn->mapping, fynpi), i++) {

		if (fynpi == fynp)
			return i;
	}

	return -1;
}

static bool fy_node_mapping_key_is_duplicate(struct fy_node *fyn, struct fy_node *fyn_key)
{
	return fy_node_mapping_lookup_pair(fyn, fyn_key) != NULL;
}

int fy_parse_document_load_node(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep, struct fy_node **fynp);

int fy_parse_document_load_alias(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep, struct fy_node **fynp)
{
	*fynp = NULL;

	fy_doc_debug(fyp, "in %s", __func__);

	/* TODO verify aliases etc */
	fy_parse_eventp_recycle(fyp, fyep);
	return 0;
}

int fy_parse_document_load_scalar(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep, struct fy_node **fynp)
{
	struct fy_node *fyn = NULL;
	struct fy_event *fye;
	struct fy_error_ctx ec;
	int rc;

	fy_error_check(fyp, fyep || !fyp->stream_error, err_out,
			"no event to process");

	FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
			fyep, err_stream_end);

	fy_doc_debug(fyp, "in %s [%s]", __func__, fy_event_type_txt[fyep->e.type]);

	*fynp = NULL;

	fye = &fyep->e;

	/* we don't free nodes that often, so no need for recycling */
	fyn = fy_node_alloc(fyd, FYNT_SCALAR);
	fy_error_check(fyp, fyn, err_out,
			"fy_node_alloc() failed");

	if (fye->type == FYET_SCALAR) {

		/* move the tags and value to the node */
		if (fye->scalar.value)
			fyn->style = fy_node_style_from_scalar_style(fye->scalar.value->scalar.style);
		else
			fyn->style = FYNS_PLAIN;
		fyn->tag = fye->scalar.tag;
		fye->scalar.tag = NULL;

		fyn->scalar = fye->scalar.value;
		fye->scalar.value = NULL;

		if (fye->scalar.anchor) {
			rc = fy_parse_document_register_anchor(fyp, fyd, fyn, fye->scalar.anchor);
			fy_error_check(fyp, !rc, err_out_rc,
					"fy_parse_document_register_anchor() failed");
		}

	} else {
		fyn->style = FYNS_ALIAS;
		fyn->scalar = fye->alias.anchor;
		fye->alias.anchor = NULL;
	}

	*fynp = fyn;
	fyn = NULL;

	/* everything OK */
	fy_parse_eventp_recycle(fyp, fyep);
	return 0;

err_out:
	rc = -1;
err_out_rc:
	fy_parse_eventp_recycle(fyp, fyep);
	fy_node_free(fyn);
	return rc;
err_stream_end:
	fy_error_report(fyp, &ec, "premature end of event stream");
	goto err_out;
}

int fy_parse_document_load_sequence(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep, struct fy_node **fynp)
{
	struct fy_node *fyn = NULL, *fyn_item = NULL;
	struct fy_event *fye = NULL;
	struct fy_token *fyt_ss = NULL;
	struct fy_error_ctx ec;
	int rc;

	fy_error_check(fyp, fyep || !fyp->stream_error, err_out,
			"no event to process");

	FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
			fyep, err_stream_end);

	fy_doc_debug(fyp, "in %s [%s]", __func__, fy_event_type_txt[fyep->e.type]);

	*fynp = NULL;

	fye = &fyep->e;

	fyt_ss = fye->sequence_start.sequence_start;

	/* we don't free nodes that often, so no need for recycling */
	fyn = fy_node_alloc(fyd, FYNT_SEQUENCE);
	fy_error_check(fyp, fyn, err_out,
			"fy_node_alloc() failed");

	fyn->style = fyt_ss && fyt_ss->type == FYTT_FLOW_SEQUENCE_START ? FYNS_FLOW : FYNS_BLOCK;

	fyn->tag = fye->sequence_start.tag;
	fye->sequence_start.tag = NULL;

	if (fye->sequence_start.anchor) {
		rc = fy_parse_document_register_anchor(fyp, fyd, fyn, fye->sequence_start.anchor);
		fy_error_check(fyp, !rc, err_out_rc,
				"fy_parse_document_register_anchor() failed");
	}

	if (fye->sequence_start.sequence_start) {
		fyn->sequence_start = fye->sequence_start.sequence_start;
		fye->sequence_start.sequence_start = NULL;
	}

	/* done with this */
	fy_parse_eventp_recycle(fyp, fyep);
	fyep = NULL;

	while ((fyep = fy_parse_private(fyp)) != NULL) {
		fye = &fyep->e;
		if (fye->type == FYET_SEQUENCE_END)
			break;

		rc = fy_parse_document_load_node(fyp, fyd, fyep, &fyn_item);
		fyep = NULL;
		fy_error_check(fyp, !rc, err_out_rc,
				"fy_parse_document_load_node() failed");

		fy_node_list_add_tail(&fyn->sequence, fyn_item);
		fyn_item = NULL;
	}

	if (fye->sequence_end.sequence_end) {
		fyn->sequence_end = fye->sequence_end.sequence_end;
		fye->sequence_end.sequence_end = NULL;
	}

	*fynp = fyn;
	fyn = NULL;

	fy_parse_eventp_recycle(fyp, fyep);
	return 0;

	/* fallthrough */
err_out:
	rc = -1;
err_out_rc:
	fy_parse_eventp_recycle(fyp, fyep);
	fy_node_free(fyn_item);
	fy_node_free(fyn);
	return rc;
err_stream_end:
	fy_error_report(fyp, &ec, "premature end of event stream");
	goto err_out;
}

int fy_parse_document_load_mapping(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep, struct fy_node **fynp)
{
	struct fy_node *fyn = NULL, *fyn_key = NULL, *fyn_value = NULL;
	struct fy_node_pair *fynp_item = NULL;
	struct fy_event *fye = NULL;
	struct fy_token *fyt_ms = NULL;
	struct fy_error_ctx ec;
	bool duplicate;
	int rc;

	fy_error_check(fyp, fyep || !fyp->stream_error, err_out,
			"no event to process");

	FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
			fyep, err_stream_end);

	fy_doc_debug(fyp, "in %s [%s]", __func__, fy_event_type_txt[fyep->e.type]);

	*fynp = NULL;

	fye = &fyep->e;

	fyt_ms = fye->mapping_start.mapping_start;

	/* we don't free nodes that often, so no need for recycling */
	fyn = fy_node_alloc(fyd, FYNT_MAPPING);
	fy_error_check(fyp, fyn, err_out,
			"fy_node_alloc() failed");

	fyn->style = fyt_ms && fyt_ms->type == FYTT_FLOW_MAPPING_START ? FYNS_FLOW : FYNS_BLOCK;

	fyn->tag = fye->mapping_start.tag;
	fye->mapping_start.tag = NULL;

	if (fye->mapping_start.anchor) {
		rc = fy_parse_document_register_anchor(fyp, fyd, fyn, fye->mapping_start.anchor);
		fy_error_check(fyp, !rc, err_out_rc,
				"fy_parse_document_register_anchor() failed");
	}

	if (fye->mapping_start.mapping_start) {
		fyn->mapping_start = fye->mapping_start.mapping_start;
		fye->mapping_start.mapping_start = NULL;
	}

	/* done with this */
	fy_parse_eventp_recycle(fyp, fyep);
	fyep = NULL;

	while ((fyep = fy_parse_private(fyp)) != NULL) {
		fye = &fyep->e;
		if (fye->type == FYET_MAPPING_END)
			break;

		fynp_item = fy_node_pair_alloc(fyd);
		fy_error_check(fyp, fynp_item, err_out,
				"fy_node_pair_alloc() failed");

		fyn_key = NULL;
		fyn_value = NULL;

		rc = fy_parse_document_load_node(fyp, fyd, fyep, &fyn_key);
		fyep = NULL;

		assert(fyn_key);

		fy_error_check(fyp, !rc, err_out_rc,
				"fy_parse_document_load_node() failed");

		/* make sure we don't add an already existing key */
		duplicate = fy_node_mapping_key_is_duplicate(fyn, fyn_key);

		FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
				!duplicate, err_duplicate_key);

		fyep = fy_parse_private(fyp);

		fy_error_check(fyp, fyep || !fyp->stream_error, err_out,
				"fy_parse_private() failed");

		FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
				fyep, err_missing_mapping_value);

		fye = &fyep->e;

		rc = fy_parse_document_load_node(fyp, fyd, fyep, &fyn_value);
		fyep = NULL;
		fy_error_check(fyp, !rc, err_out_rc,
				"fy_parse_document_load_node() failed");

		assert(fyn_value);

		fynp_item->key = fyn_key;
		fynp_item->value = fyn_value;
		fy_node_pair_list_add_tail(&fyn->mapping, fynp_item);
		fynp_item = NULL;
		fyn_key = NULL;
		fyn_value = NULL;
	}

	if (fye->mapping_end.mapping_end) {
		fyn->mapping_end = fye->mapping_end.mapping_end;
		fye->mapping_end.mapping_end = NULL;
	}

	*fynp = fyn;
	fyn = NULL;

	fy_parse_eventp_recycle(fyp, fyep);

	return 0;

err_out:
	rc = -1;
err_out_rc:
	fy_parse_eventp_recycle(fyp, fyep);
	fy_node_pair_free(fynp_item);
	fy_node_free(fyn_key);
	fy_node_free(fyn_value);
	fy_node_free(fyn);
	return rc;

err_duplicate_key:
	ec.start_mark = *fy_node_get_start_mark(fyn_key);
	ec.end_mark = *fy_node_get_end_mark(fyn_key);
	ec.fyi = fy_node_get_input(fyn_key);
	fy_error_report(fyp, &ec, "duplicate key");
	goto err_out;

err_missing_mapping_value:
	fy_error_report(fyp, &ec, "missing mapping value");
	goto err_out;
err_stream_end:
	fy_error_report(fyp, &ec, "premature end of event stream");
	goto err_out;
}

int fy_parse_document_load_node(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep, struct fy_node **fynp)
{
	struct fy_event *fye;
	enum fy_event_type type;
	struct fy_error_ctx ec;

	*fynp = NULL;

	fy_error_check(fyp, fyep || !fyp->stream_error, err_out,
			"no event to process");

	FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
			fyep, err_stream_end);

	fy_doc_debug(fyp, "in %s [%s]", __func__, fy_event_type_txt[fyep->e.type]);

	fye = &fyep->e;

	type = fye->type;

	FY_ERROR_CHECK(fyp, fy_document_event_get_token(fye), &ec, FYEM_DOC,
			type == FYET_ALIAS || type == FYET_SCALAR ||
			type == FYET_SEQUENCE_START || type == FYET_MAPPING_START,
			err_bad_event);

	switch (type) {

	case FYET_ALIAS:
	case FYET_SCALAR:
		return fy_parse_document_load_scalar(fyp, fyd, fyep, fynp);

	case FYET_SEQUENCE_START:
		return fy_parse_document_load_sequence(fyp, fyd, fyep, fynp);

	case FYET_MAPPING_START:
		return fy_parse_document_load_mapping(fyp, fyd, fyep, fynp);

	default:
		break;
	}

err_out:
	fy_parse_eventp_recycle(fyp, fyep);
	return -1;

err_bad_event:
	fy_error_report(fyp, &ec, "bad event");
	goto err_out;

err_stream_end:
	fy_error_report(fyp, &ec, "premature end of event stream");
	goto err_out;
}

int fy_parse_document_load_end(struct fy_parser *fyp, struct fy_document *fyd, struct fy_eventp *fyep)
{
	struct fy_event *fye;
	struct fy_error_ctx ec;
	int rc;

	fy_error_check(fyp, fyep || !fyp->stream_error, err_out,
			"no event to process");

	FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
			fyep, err_stream_end);

	fy_doc_debug(fyp, "in %s [%s]", __func__, fy_event_type_txt[fyep->e.type]);

	fye = &fyep->e;

	FY_ERROR_CHECK(fyp, fy_document_event_get_token(fye), &ec, FYEM_DOC,
			fye->type == FYET_DOCUMENT_END,
			err_bad_event);

	return 0;
err_out:
	rc = -1;
	fy_parse_eventp_recycle(fyp, fyep);
	return rc;

err_bad_event:
	fy_error_report(fyp, &ec, "bad event");
	goto err_out;

err_stream_end:
	fy_error_report(fyp, &ec, "premature end of event stream");
	goto err_out;
}

struct fy_document *fy_parse_load_document(struct fy_parser *fyp)
{
	struct fy_document *fyd = NULL;
	struct fy_eventp *fyep = NULL;
	struct fy_event *fye = NULL;
	struct fy_error_ctx ec;
	int rc;
	bool was_stream_start;

again:
	was_stream_start = false;
	do {
		/* get next event */
		fyep = fy_parse_private(fyp);

		/* no more */
		if (!fyep)
			return NULL;

		was_stream_start = fyep->e.type == FYET_STREAM_START;

		if (was_stream_start) {
			fy_parse_eventp_recycle(fyp, fyep);
			fyep = NULL;
		}

	} while (was_stream_start);

	fye = &fyep->e;

	/* STREAM_END */
	if (fye->type == FYET_STREAM_END) {
		fy_parse_eventp_recycle(fyp, fyep);

		/* final STREAM_END? */
		if (fyp->state == FYPS_END)
			return NULL;

		/* multi-stream */
		goto again;
	}

	FY_ERROR_CHECK(fyp, fy_document_event_get_token(fye), &ec, FYEM_DOC,
			fye->type == FYET_DOCUMENT_START,
			err_bad_event);

	fyd = fy_parse_document_create(fyp, fyep);
	fyep = NULL;

	fy_error_check(fyp, fyd, err_out,
			"fy_parse_document_create() failed");

	fy_doc_debug(fyp, "calling load_node() for root");
	rc = fy_parse_document_load_node(fyp, fyd, fy_parse_private(fyp), &fyd->root);
	fy_error_check(fyp, !rc, err_out,
			"fy_parse_document_load_node() failed");

	rc = fy_parse_document_load_end(fyp, fyd, fy_parse_private(fyp));
	fy_error_check(fyp, !rc, err_out,
			"fy_parse_document_load_node() failed");

	/* always resolve parents */
	fy_resolve_parent_node(fyd, fyd->root, NULL);

	if (fyp->cfg.flags & FYPCF_RESOLVE_DOCUMENT) {
		rc = fy_document_resolve(fyd);
		fy_error_check(fyp, !rc, err_out,
				"fy_document_resolve() failed");
	}

	return fyd;

err_out:
	fy_parse_eventp_recycle(fyp, fyep);
	fy_parse_document_destroy(fyp, fyd);
	return NULL;

err_bad_event:
	fy_error_report(fyp, &ec, "bad event");
	goto err_out;

}

struct fy_node *fy_node_copy(struct fy_document *fyd, struct fy_node *fyn_from)
{
	struct fy_parser *fyp;
	struct fy_document *fyd_from;
	struct fy_node *fyn, *fyni, *fynit;
	struct fy_node_pair *fynp, *fynpt;
	struct fy_anchor *fya, *fya_from;
	const char *anchor;
	size_t anchor_len;
	int rc;

	if (!fyd || !fyn_from || !fyn_from->fyd)
		return NULL;

	fyp = fyd->fyp;

	fyd_from = fyn_from->fyd;

	fyn = fy_node_alloc(fyd, fyn_from->type);
	fy_error_check(fyd->fyp, fyn, err_out,
			"fy_node_alloc() failed");

	fyn->tag = fy_token_ref(fyn_from->tag);
	fyn->style = fyn_from->style;

	switch (fyn->type) {
	case FYNT_SCALAR:
		fyn->scalar = fy_token_ref(fyn_from->scalar);
		break;

	case FYNT_SEQUENCE:
		for (fyni = fy_node_list_head(&fyn_from->sequence); fyni;
				fyni = fy_node_next(&fyn_from->sequence, fyni)) {

			fynit = fy_node_copy(fyd, fyni);
			fy_error_check(fyp, fynit, err_out,
					"fy_node_copy() failed");

			fy_node_list_add_tail(&fyn->sequence, fynit);
		}

		break;
	case FYNT_MAPPING:
		for (fynp = fy_node_pair_list_head(&fyn_from->mapping); fynp;
				fynp = fy_node_pair_next(&fyn_from->mapping, fynp)) {

			fynpt = fy_node_pair_alloc(fyd);
			fy_error_check(fyp, fynpt, err_out,
					"fy_node_pair_alloc() failed");

			fynpt->key = fy_node_copy(fyd, fynp->key);
			fynpt->value = fy_node_copy(fyd, fynp->value);

			fy_node_pair_list_add_tail(&fyn->mapping, fynpt);
		}
		break;
	}

	/* drop an anchor to the copy */
	for (fya_from = fy_anchor_list_head(&fyd_from->anchors); fya_from;
			fya_from = fy_anchor_next(&fyd_from->anchors, fya_from)) {
		if (fyn_from == fya_from->fyn)
			break;
	}

	/* source node has an anchor */
	if (fya_from) {
		fya = fy_document_lookup_anchor_by_token(fyd, fya_from->anchor);
		if (!fya) {
			/* update the new anchor position */
			rc = fy_parse_document_register_anchor(fyp, fyd, fyn, fya_from->anchor);
			fy_error_check(fyp, !rc, err_out,
					"fy_parse_document_register_anchor() failed");

			fy_anchor_list_add(&fyd->anchors, fya);
		} else {
			anchor = fy_anchor_get_text(fya, &anchor_len);
			fy_error_check(fyp, anchor, err_out,
					"fy_anchor_get_text() failed");
			fy_doc_debug(fyp, "not overwritting anchor %.*s", (int)anchor_len, anchor);
		}
	}

	return fyn;

err_out:
	return NULL;
}

int fy_node_copy_to_scalar(struct fy_document *fyd, struct fy_node *fyn_to, struct fy_node *fyn_from)
{
	struct fy_node *fyn, *fyni;
	struct fy_node_pair *fynp;

	fyn = fy_node_copy(fyd, fyn_from);
	if (!fyn)
		return -1;

	/* the node is guaranteed to be a scalar */
	fy_token_unref(fyn_to->tag);
	fyn_to->tag = NULL;
	fy_token_unref(fyn_to->scalar);
	fyn_to->scalar = NULL;

	fyn_to->type = fyn->type;
	fyn_to->tag = fy_token_ref(fyn->tag);
	fyn_to->style = fyn->style;

	switch (fyn->type) {
	case FYNT_SCALAR:
		fyn_to->scalar = fyn->scalar;
		fyn->scalar = NULL;
		break;
	case FYNT_SEQUENCE:
		fy_node_list_init(&fyn_to->sequence);
		while ((fyni = fy_node_list_pop(&fyn->sequence)) != NULL)
			fy_node_list_add_tail(&fyn_to->sequence, fyni);
		break;
	case FYNT_MAPPING:
		fy_node_pair_list_init(&fyn_to->mapping);
		while ((fynp = fy_node_pair_list_pop(&fyn->mapping)) != NULL)
			fy_node_pair_list_add_tail(&fyn_to->mapping, fynp);
		break;
	}

	/* and free */
	fy_node_free(fyn);

	return 0;
}

int fy_node_insert(struct fy_node *fyn_to, struct fy_node *fyn_from)
{
	struct fy_document *fyd;
	struct fy_parser *fyp;
	struct fy_node *fyn_parent, *fyn_cpy, *fyni, *fyn_prev;
	struct fy_node_pair *fynp, *fynpi, *fynpj;
	int rc;

	if (!fyn_to || !fyn_to->fyd)
		return -1;

	fyd = fyn_to->fyd;
	assert(fyd);
	fyp = fyd->fyp;
	assert(fyp);

	fyn_parent = fyn_to->parent;
	fynp = NULL;
	if (fyn_parent) {
		fy_error_check(fyp, fyn_parent->type != FYNT_SCALAR, err_out,
				    "Illegal scalar parent node type");

		fy_error_check(fyp, fyn_from, err_out,
				"Illegal NULL source node");

		if (fyn_parent->type == FYNT_MAPPING) {
			/* find mapping pair that contains the `to` node */
			for (fynp = fy_node_pair_list_head(&fyn_parent->mapping); fynp;
					fynp = fy_node_pair_next(&fyn_parent->mapping, fynp)) {
				if (fynp->value == fyn_to)
					break;
			}
		}
	}

	/* verify no funkiness on root */
	assert(fyn_parent || fyn_to == fyd->root);

	/* deleting target */
	if (!fyn_from) {
		fyn_to->parent = NULL;

		if (!fyn_parent) {
			fy_doc_debug(fyp, "Deleting root node");
			fy_node_free(fyn_to);
			fyd->root = NULL;
		} else if (fyn_parent->type == FYNT_SEQUENCE) {
			fy_doc_debug(fyp, "Deleting sequence node");
			fy_node_list_del(&fyn_parent->sequence, fyn_to);
			fy_node_free(fyn_to);
		} else {
			fy_doc_debug(fyp, "Deleting mapping node");
			/* should never happen, it's checked right above, but play safe */
			assert(fyn_parent->type == FYNT_MAPPING);

			fy_error_check(fyp, fynp, err_out,
					"Illegal mapping node found");

			fy_node_pair_list_del(&fyn_parent->mapping, fynp);
			/* this will also delete fyn_to */
			fy_node_pair_free(fynp);
		}
		return 0;
	}

	/*
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
	 */

	/* if types of `from` and `to` differ (or it's a scalar), it's a replace */
	if (fyn_from->type != fyn_to->type || fyn_from->type == FYNT_SCALAR) {

		fyn_cpy = fy_node_copy(fyd, fyn_from);
		fy_error_check(fyp, fyn_cpy, err_out,
				"fy_node_copy() failed");

		if (!fyn_parent) {
			fy_doc_debug(fyp, "Replacing root node");
			fy_node_free(fyd->root);
			fyd->root = fyn_cpy;
		} else if (fyn_parent->type == FYNT_SEQUENCE) {
			fy_doc_debug(fyp, "Replacing sequence node");

			/* get previous */
			fyn_prev = fy_node_prev(&fyn_parent->sequence, fyn_to);

			/* delete */
			fy_node_list_del(&fyn_parent->sequence, fyn_to);
			fy_node_free(fyn_to);

			/* if there's no previous insert to head */
			if (!fyn_prev)
				fy_node_list_add(&fyn_parent->sequence, fyn_cpy);
			else
				fy_node_list_insert_after(&fyn_parent->sequence, fyn_prev, fyn_cpy);
		} else {
			fy_doc_debug(fyp, "Replacing mapping node value");
			/* should never happen, it's checked right above, but play safe */
			assert(fyn_parent->type == FYNT_MAPPING);
			fy_error_check(fyp, fynp, err_out,
					"Illegal mapping node found");

			if (fynp->value)
				fy_node_free(fynp->value);
			fynp->value = fyn_cpy;
		}

		return 0;
	}

	/* types match, if it's a sequence append */
	if (fyn_to->type == FYNT_SEQUENCE) {

		fy_doc_debug(fyp, "Appending to sequence node");

		for (fyni = fy_node_list_head(&fyn_from->sequence); fyni;
				fyni = fy_node_next(&fyn_from->sequence, fyni)) {

			fyn_cpy = fy_node_copy(fyd, fyni);
			fy_error_check(fyp, fyn_cpy, err_out,
					"fy_node_copy() failed");

			fy_node_list_add_tail(&fyn_to->sequence, fyn_cpy);
		}
	} else {
		/* only mapping is possible here */

		/* iterate over all the keys in the `from` */
		for (fynpi = fy_node_pair_list_head(&fyn_from->mapping); fynpi;
			fynpi = fy_node_pair_next(&fyn_from->mapping, fynpi)) {

			/* find whether the key already exists */
			for (fynpj = fy_node_pair_list_head(&fyn_to->mapping); fynpj;
				fynpj = fy_node_pair_next(&fyn_to->mapping, fynpj)) {

				if (fy_node_compare(fynpi->key, fynpj->key))
					break;
			}

			if (!fynpj) {
				fy_doc_debug(fyp, "Appending to mapping node");

				/* not found? append it */
				fynpj = fy_node_pair_alloc(fyd);
				fy_error_check(fyp, fynpj, err_out,
						"fy_node_pair_alloc() failed");

				fynpj->key = fy_node_copy(fyd, fynpi->key);
				fy_error_check(fyp, !fynpi->key || fynpj->key, err_out,
						"fy_node_copy() failed");
				fynpj->value = fy_node_copy(fyd, fynpi->value);
				fy_error_check(fyp, !fynpi->value || fynpj->value, err_out,
						"fy_node_copy() failed");

				fy_node_pair_list_add_tail(&fyn_to->mapping, fynpj);

			} else {

				fy_doc_debug(fyp, "Updating mapping node value");

				/* found? replace value */
				fy_node_free(fynpj->value);
				fynpj->value = fy_node_copy(fyd, fynpi->value);
				fy_error_check(fyp, !fynpi->value || fynpj->value, err_out,
						"fy_node_copy() failed");
			}
		}
	}

	/* if the documents differ, merge their states */
	if (fyn_to->fyd != fyn_from->fyd) {
		rc = fy_document_state_merge(fyn_to->fyd, fyn_from->fyd);
		if (rc)
			return rc;
	}

	return 0;

err_out:
	return -1;
}

int fy_document_insert_at(struct fy_document *fyd, const char *path, struct fy_node *fyn)
{
	int rc;

	rc = fy_node_insert(
		fy_node_by_path(fy_document_root(fyd), path),
		fyn);

	fy_node_free(fyn);

	return rc;
}

static int fy_document_node_update_tags(struct fy_document *fyd, struct fy_node *fyn)
{
	struct fy_parser *fyp;
	struct fy_node *fyni;
	struct fy_node_pair *fynp, *fynpi;
	struct fy_token *fyt_td;
	const char *handle;
	size_t handle_size;
	int rc;

	if (!fyd || !fyn || !fyd->fyp)
		return 0;

	fyp = fyd->fyp;

	/* replace tag reference with the one that the document contains */
	if (fyn->tag) {
		fy_error_check(fyp, fyn->tag->type == FYTT_TAG, err_out,
				"bad node tag");

		handle = fy_tag_directive_token_handle(fyn->tag->tag.fyt_td, &handle_size);
		fy_error_check(fyp, handle, err_out,
				"bad tag directive token");

		fyt_td = fy_document_state_lookup_tag_directive(fyd->fyds, handle, handle_size);
		fy_error_check(fyp, fyt_td, err_out,
				"Missing tag directive with handle=%.*s", (int)handle_size, handle);

		/* need to replace this */
		if (fyt_td != fyn->tag->tag.fyt_td) {
			fy_token_unref(fyn->tag->tag.fyt_td);
			fyn->tag->tag.fyt_td = fy_token_ref(fyt_td);

		}
	}


	switch (fyn->type) {
	case FYNT_SCALAR:
		break;

	case FYNT_SEQUENCE:
		for (fyni = fy_node_list_head(&fyn->sequence); fyni;
				fyni = fy_node_next(&fyn->sequence, fyni)) {

			rc = fy_document_node_update_tags(fyd, fyni);
			if (rc)
				goto err_out_rc;
		}
		break;

	case FYNT_MAPPING:
		for (fynp = fy_node_pair_list_head(&fyn->mapping); fynp; fynp = fynpi) {

			fynpi = fy_node_pair_next(&fyn->mapping, fynp);

			/* the parent of the key is always NULL */
			rc = fy_document_node_update_tags(fyd, fynp->key);
			if (rc)
				goto err_out_rc;

			rc = fy_document_node_update_tags(fyd, fynp->value);
			if (rc)
				goto err_out_rc;
		}
		break;
	}

	return 0;

err_out:
	rc = -1;
err_out_rc:
	return rc;
}

void fy_document_dump_tag_directives(struct fy_document *fyd, const char *banner)
{
	struct fy_document_state *fyds;
	struct fy_token *fyt;
	const char *handle, *prefix;
	size_t handle_size, prefix_size;

	if (!fyd || !fyd->fyds)
		return;

	fyds = fyd->fyds;

	for (fyt = fy_token_list_first(&fyds->fyt_td); fyt; fyt = fy_token_next(&fyds->fyt_td, fyt)) {

		handle = fy_tag_directive_token_handle(fyt, &handle_size);
		assert(handle);

		prefix = fy_tag_directive_token_prefix(fyt, &prefix_size);
		assert(prefix);

		fy_notice(fyd->fyp, "%s tag directive \"%.*s\" \"%.*s\"", banner,
				(int)handle_size, handle,
				(int)prefix_size, prefix);
	}
}

struct fy_token *fy_document_tag_directive_iterate(struct fy_document *fyd, void **prevp)
{
	struct fy_token_list *fytl;

	if (!fyd || !fyd->fyds || !prevp)
		return NULL;

	fytl = &fyd->fyds->fyt_td;

	return *prevp = *prevp ? fy_token_next(fytl, *prevp) : fy_token_list_head(fytl);
}

struct fy_token *fy_document_tag_directive_lookup(struct fy_document *fyd, const char *handle)
{
	struct fy_token *fyt;
	void *iter;
	const char *h;
	size_t h_size, len;

	if (!fyd || !handle)
		return NULL;
	len = strlen(handle);

	iter = NULL;
	while ((fyt = fy_document_tag_directive_iterate(fyd, &iter)) != NULL) {
		h = fy_tag_directive_token_handle(fyt, &h_size);
		if (!h)
			continue;
		if (h_size == len && !memcmp(h, handle, len))
			return fyt;
	}
	return NULL;
}

int fy_document_tag_directive_add(struct fy_document *fyd, const char *handle, const char *prefix)
{
	struct fy_token *fyt;

	if (!fyd || !fyd->fyds || !handle || !prefix)
		return -1;

	/* it must not exist */
	fyt = fy_document_tag_directive_lookup(fyd, handle);
	if (fyt)
		return -1;

	return fy_append_tag_directive(fyd->fyp, fyd->fyds, handle, prefix);
}

int fy_document_tag_directive_remove(struct fy_document *fyd, const char *handle)
{
	struct fy_token *fyt;

	if (!fyd || !fyd->fyds || !handle)
		return -1;

	/* it must not exist */
	fyt = fy_document_tag_directive_lookup(fyd, handle);
	if (!fyt || fyt->refs != 1)
		return -1;

	fy_token_list_del(&fyd->fyds->fyt_td, fyt);
	fy_token_unref(fyt);

	return 0;
}

int fy_document_state_merge(struct fy_document *fyd, struct fy_document *fydc)
{
	struct fy_parser *fyp = fyd->fyp;
	struct fy_document_state *fyds, *fydsc;
	const char *td_prefix, *tdc_handle, *tdc_prefix;
	size_t td_prefix_size, tdc_handle_size, tdc_prefix_size;
	struct fy_error_ctx ec;
	struct fy_token *fyt, *fytc_td, *fyt_td;
	int rc;

	/* both the document and the parser object must exist */
	if (!fyd || !fydc)
		return 0;

	fyds = fyd->fyds;
	assert(fyds);

	fydsc = fydc->fyds;
	assert(fydsc);

	/* check if there's a duplicate handle (which differs */
	for (fytc_td = fy_token_list_first(&fydsc->fyt_td); fytc_td; fytc_td = fy_token_next(&fydsc->fyt_td, fytc_td)) {

		tdc_handle = fy_tag_directive_token_handle(fytc_td, &tdc_handle_size);
		assert(tdc_handle);

		tdc_prefix = fy_tag_directive_token_prefix(fytc_td, &tdc_prefix_size);
		assert(tdc_prefix);

		fyt_td = fy_document_state_lookup_tag_directive(fyds, tdc_handle, tdc_handle_size);
		if (fyt_td) {
			/* exists, must check whether the prefixes match */
			td_prefix = fy_tag_directive_token_prefix(fyt_td, &td_prefix_size);
			assert(td_prefix);

			/* match? do nothing */
			if (tdc_prefix_size == td_prefix_size &&
			    !memcmp(tdc_prefix, td_prefix, td_prefix_size)) {
				fy_notice(fyp, "matching tag directive \"%.*s\" \"%.*s\"",
						(int)tdc_handle_size, tdc_handle,
						(int)tdc_prefix_size, tdc_prefix);
				continue;
			}

			/* the tag directive must be overridable */
			FY_ERROR_CHECK(fyp, fytc_td, &ec, FYEM_DOC,
					fy_token_tag_directive_is_overridable(fyt_td),
					err_dup_diff_tag);

			/* override tag directive */
			fy_token_list_del(&fyds->fyt_td, fyt_td);
			fy_token_unref(fyt_td);

			fy_notice(fyp, "overriding tag directive \"%.*s\" \":%.*s\"",
					(int)tdc_handle_size, tdc_handle,
					(int)tdc_prefix_size, tdc_prefix);
		} else {
			fy_notice(fyp, "appending tag directive \"%.*s\" \"%.*s\"",
					(int)tdc_handle_size, tdc_handle,
					(int)tdc_prefix_size, tdc_prefix);
		}

		fyt = fy_token_create(fyp, FYTT_TAG_DIRECTIVE,
				&fytc_td->handle,
				fytc_td->tag_directive.tag_length,
				fytc_td->tag_directive.uri_length);
		fy_error_check(fyp, fyt, err_out,
				"fy_token_create() failed");

		fy_token_list_add_tail(&fyds->fyt_td, fyt);
	}

	rc = fy_document_node_update_tags(fyd, fy_document_root(fyd));
	fy_error_check(fyp, !rc, err_out_rc,
			"fy_document_node_update_tags() failed");

	/* merge other document state */
	fyd->fyds->version_explicit |= fydc->fyds->version_explicit;
	fyd->fyds->tags_explicit |= fydc->fyds->tags_explicit;

	if (fyd->fyds->version.major < fydc->fyds->version.major ||
	    (fyd->fyds->version.major == fydc->fyds->version.major &&
		fyd->fyds->version.minor < fydc->fyds->version.minor))
		fyd->fyds->version = fydc->fyds->version;

	return 0;

err_out:
	rc = -1;
err_out_rc:
	return rc;

err_dup_diff_tag:
	fy_error_report(fyp, &ec, "duplicate differing tag declaration");
	goto err_out;
}

static bool fy_node_is_alias(struct fy_node *fyn)
{
	return fyn && fyn->type == FYNT_SCALAR && fyn->style == FYNS_ALIAS;
}

static int fy_resolve_alias(struct fy_document *fyd, struct fy_node *fyn)
{
	struct fy_parser *fyp = fyd->fyp;
	struct fy_error_ctx ec;
	struct fy_anchor *fya;
	int rc;

	fya = fy_document_lookup_anchor_by_token(fyd, fyn->scalar);

	FY_ERROR_CHECK(fyp, fyn->scalar, &ec, FYEM_DOC,
			fya,
			err_bad_alias);

	rc = fy_node_copy_to_scalar(fyd, fyn, fya->fyn);
	fy_error_check(fyp, !rc, err_out,
			"fy_node_copy_to_scalar() failed");

	return 0;

err_out:
	return -1;

err_bad_alias:
	fy_error_report(fyp, &ec, "invalid alias");
	goto err_out;
}

static bool fy_node_pair_is_merge_key(struct fy_node_pair *fynp)
{
	struct fy_node *fyn = fynp->key;

	return fyn && fyn->type == FYNT_SCALAR && fyn->style == FYNS_PLAIN &&
	       fy_plain_atom_streq(fy_token_atom(fyn->scalar), "<<");
}

static struct fy_node *fy_alias_get_merge_mapping(struct fy_document *fyd, struct fy_node *fyn)
{
	struct fy_anchor *fya;

	/* must be an alias */
	if (!fy_node_is_alias(fyn))
		return NULL;

	/* anchor must exist */
	fya = fy_document_lookup_anchor_by_token(fyd, fyn->scalar);
	if (!fya)
		return NULL;

	/* and it must be a mapping */
	if (fya->fyn->type != FYNT_MAPPING)
		return NULL;

	return fya->fyn;
}

static bool fy_node_pair_is_valid_merge_key(struct fy_document *fyd, struct fy_node_pair *fynp)
{
	struct fy_node *fyn, *fyni, *fynm;

	fyn = fynp->value;

	/* value must exist */
	if (!fyn)
		return false;

	/* scalar alias */
	fynm = fy_alias_get_merge_mapping(fyd, fyn);
	if (fynm)
		return true;

	/* it must be a sequence then */
	if (fyn->type != FYNT_SEQUENCE)
		return false;

	/* the sequence must only contain valid aliases for mapping */
	for (fyni = fy_node_list_head(&fyn->sequence); fyni;
			fyni = fy_node_next(&fyn->sequence, fyni)) {

		/* sequence of aliases only! */
		fynm = fy_alias_get_merge_mapping(fyd, fyni);
		if (!fynm)
			return false;

	}

	return true;
}

static int fy_resolve_merge_key_populate(struct fy_document *fyd, struct fy_node *fyn,
					  struct fy_node_pair *fynp, struct fy_node *fynm)
{
	struct fy_node_pair *fynpi, *fynpn;

	if (!fyd)
		return -1;

	fy_error_check(fyd->fyp,
			fyn && fynp && fynm && fyn->type == FYNT_MAPPING && fynm->type == FYNT_MAPPING,
			err_out, "bad inputs to %s", __func__);

	for (fynpi = fy_node_pair_list_head(&fynm->mapping); fynpi;
		fynpi = fy_node_pair_next(&fynm->mapping, fynpi)) {

		/* make sure we don't override an already existing key */
		if (fy_node_mapping_key_is_duplicate(fyn, fynpi->key))
			continue;

		fynpn = fy_node_pair_alloc(fyd);
		fy_error_check(fyd->fyp, fynpn, err_out,
				"fy_node_pair_alloc() failed");

		fynpn->key = fy_node_copy(fyd, fynpi->key);
		fynpn->value = fy_node_copy(fyd, fynpi->value);


		fy_node_pair_list_insert_after(&fyn->mapping, fynp, fynpn);
	}

	return 0;

err_out:
	return -1;
}

static int fy_resolve_merge_key(struct fy_document *fyd, struct fy_node *fyn, struct fy_node_pair *fynp)
{
	struct fy_parser *fyp = fyd->fyp;
	struct fy_node *fynv, *fyni, *fynm;
	struct fy_error_ctx ec;
	int rc;

	/* it must be a valid merge key value */
	FY_ERROR_CHECK(fyp, NULL, &ec, FYEM_DOC,
			fy_node_pair_is_valid_merge_key(fyd, fynp),
			err_invalid_merge_key);

	fynv = fynp->value;
	fynm = fy_alias_get_merge_mapping(fyd, fynv);
	if (fynm) {
		rc = fy_resolve_merge_key_populate(fyd, fyn, fynp, fynm);
		fy_error_check(fyp, !rc, err_out_rc,
				"fy_resolve_merge_key_populate() failed");

		return 0;
	}

	/* it must be a sequence then */
	fy_error_check(fyp, fynv->type == FYNT_SEQUENCE, err_out,
			"invalid node type to use for merge key");

	/* the sequence must only contain valid aliases for mapping */
	for (fyni = fy_node_list_head(&fynv->sequence); fyni;
			fyni = fy_node_next(&fynv->sequence, fyni)) {

		fynm = fy_alias_get_merge_mapping(fyd, fyni);
		fy_error_check(fyp, fynm, err_out,
				"invalid merge key sequence item (not an alias)");

		rc = fy_resolve_merge_key_populate(fyd, fyn, fynp, fynm);
		fy_error_check(fyp, !rc, err_out_rc,
				"fy_resolve_merge_key_populate() failed");
	}

	return 0;

err_out:
	rc = -1;
err_out_rc:
	return rc;

err_invalid_merge_key:
	ec.start_mark = *fy_node_get_start_mark(fynp->value);
	ec.end_mark = *fy_node_get_end_mark(fynp->value);
	ec.fyi = fy_node_get_input(fynp->value);
	fy_error_report(fyp, &ec, "invalid merge key value");
	goto err_out;
}

/* the anchors are scalars that have the FYNS_ALIAS style */
static int fy_resolve_anchor_node(struct fy_document *fyd, struct fy_node *fyn)
{
	struct fy_node *fyni;
	struct fy_node_pair *fynp, *fynpi;
	int rc, ret_rc = 0;

	if (!fyn)
		return 0;

	if (fy_node_is_alias(fyn))
		return fy_resolve_alias(fyd, fyn);

	if (fyn->type == FYNT_SEQUENCE) {

		for (fyni = fy_node_list_head(&fyn->sequence); fyni;
				fyni = fy_node_next(&fyn->sequence, fyni)) {

			rc = fy_resolve_anchor_node(fyd, fyni);
			if (rc && !ret_rc)
				ret_rc = rc;
		}

	} else if (fyn->type == FYNT_MAPPING) {

		for (fynp = fy_node_pair_list_head(&fyn->mapping); fynp; fynp = fynpi) {

			fynpi = fy_node_pair_next(&fyn->mapping, fynp);

			if (fy_node_pair_is_merge_key(fynp)) {
				rc = fy_resolve_merge_key(fyd, fyn, fynp);
				if (rc && !ret_rc)
					ret_rc = rc;

				/* remove this node pair */
				if (!rc) {
					fy_node_pair_list_del(&fyn->mapping, fynp);
					fy_node_pair_free(fynp);
				}

			} else {
				rc = fy_resolve_anchor_node(fyd, fynp->key);
				if (rc && !ret_rc)
					ret_rc = rc;

				rc = fy_resolve_anchor_node(fyd, fynp->value);
				if (rc && !ret_rc)
					ret_rc = rc;
			}

		}
	}

	return ret_rc;
}

static void fy_resolve_parent_node(struct fy_document *fyd, struct fy_node *fyn, struct fy_node *fyn_parent)
{
	struct fy_node *fyni;
	struct fy_node_pair *fynp, *fynpi;

	if (!fyn)
		return;

	fyn->parent = fyn_parent;

	switch (fyn->type) {
	case FYNT_SCALAR:
		break;

	case FYNT_SEQUENCE:
		for (fyni = fy_node_list_head(&fyn->sequence); fyni;
				fyni = fy_node_next(&fyn->sequence, fyni)) {

			fy_resolve_parent_node(fyd, fyni, fyn);
		}
		break;

	case FYNT_MAPPING:
		for (fynp = fy_node_pair_list_head(&fyn->mapping); fynp; fynp = fynpi) {

			fynpi = fy_node_pair_next(&fyn->mapping, fynp);

			/* the parent of the key is always NULL */
			fy_resolve_parent_node(fyd, fynp->key, NULL);
			fy_resolve_parent_node(fyd, fynp->value, fyn);
			fynp->parent = fyn;
		}
		break;
	}
}

int fy_document_resolve(struct fy_document *fyd)
{
	int rc;

	if (!fyd)
		return 0;

	rc = fy_resolve_anchor_node(fyd, fyd->root);

	/* redo parent resolution */
	fy_resolve_parent_node(fyd, fyd->root, NULL);

	return rc;
}

void fy_document_free_nodes(struct fy_document *fyd)
{
	struct fy_document *fyd_child;

	for (fyd_child = fy_document_list_first(&fyd->children); fyd_child; fyd_child = fy_document_next(&fyd->children, fyd_child)) {
		fy_document_free_nodes(fyd_child);
	}

	fy_node_free(fyd->root);
	fyd->root = NULL;
}

void fy_document_destroy(struct fy_document *fyd)
{
	struct fy_document *fyd_child;
	struct fy_parser *fyp;
	bool owns_parser;

	/* both the document and the parser object must exist */
	if (!fyd || !fyd->fyp)
		return;

	/* we have to free the nodes first */
	fy_document_free_nodes(fyd);

	/* recursively delete children */
	while ((fyd_child = fy_document_list_pop(&fyd->children)) != NULL) {
		fyd_child->parent = NULL;
		fy_document_destroy(fyd_child);
	}

	fyp = fyd->fyp;
	owns_parser = fyd->owns_parser;

	fy_parse_document_destroy(fyp, fyd);

	if (owns_parser)
		fy_parser_destroy(fyp);
}

int fy_document_set_parent(struct fy_document *fyd, struct fy_document *fyd_child)
{

	if (!fyd || !fyd_child || fyd_child->parent)
		return -1;

	fyd_child->parent = fyd;
	fy_document_list_add_tail(&fyd->children, fyd_child);

	return 0;
}

static const struct fy_parse_cfg doc_parse_default_cfg = {
	.search_path = "",
	.flags = FYPCF_QUIET | FYPCF_DEBUG_LEVEL_WARNING |
		 FYPCF_DEBUG_DIAG_TYPE | FYPCF_COLOR_NONE,
};

struct fy_document *fy_document_create(const struct fy_parse_cfg *cfg)
{
	struct fy_parser *fyp = NULL;
	struct fy_document *fyd = NULL;

	if (!cfg)
		cfg = &doc_parse_default_cfg;

	fyp = fy_parser_create(cfg);
	if (!fyp)
		return NULL;

	fyd = fy_parse_alloc(fyp, sizeof(*fyd));
	fy_error_check(fyp, fyd, err_out,
		"fy_parse_alloc() failed");
	memset(fyd, 0, sizeof(*fyd));

	fyd->fyp = fyp;
	fy_talloc_list_init(&fyd->tallocs);

	fy_anchor_list_init(&fyd->anchors);
	fyd->root = NULL;

	fyd->fyds = fy_document_state_ref(fyp->current_document_state);
	fy_error_check(fyp, fyd->fyds, err_out,
			"fy_document_state_ref() failed");
	fyp->external_document_state = true;	/* parser will not update state */

	fyd->owns_parser = true;

	fyd->errfp = NULL;
	fyd->errbuf = NULL;
	fyd->errsz = 0;

	fy_document_list_init(&fyd->children);

	return fyd;

err_out:
	fy_parse_document_destroy(fyp, fyd);
	fy_parser_destroy(fyp);
	return NULL;
}

struct fy_document_vbuildf_ctx {
	const char *fmt;
	va_list ap;
};

static int parser_setup_from_string(struct fy_parser *fyp, void *user)
{
	return fy_parser_set_string(fyp, user);
}

static int parser_setup_from_file(struct fy_parser *fyp, void *user)
{
	return fy_parser_set_input_file(fyp, user);
}

static int parser_setup_from_fp(struct fy_parser *fyp, void *user)
{
	return fy_parser_set_input_fp(fyp, NULL, user);
}

static int parser_setup_from_fmt_ap(struct fy_parser *fyp, void *user)
{
	struct fy_document_vbuildf_ctx *vctx = user;
	va_list ap, ap_orig;
	int size, sizew;
	char *buf;

	/* first try without allocating */
	va_copy(ap_orig, vctx->ap);
	size = vsnprintf(NULL, 0, vctx->fmt, ap_orig);
	va_end(ap_orig);

	fy_error_check(fyp, size >= 0, err_out,
			"vsnprintf() failed");

	/* the buffer will stick around until the parser is destroyed */
	buf = fy_parser_alloc(fyp, size + 1);
	fy_error_check(fyp, buf, err_out,
			"fy_parser_alloc() failed");

	va_copy(ap, vctx->ap);
	sizew = vsnprintf(buf, size + 1, vctx->fmt, ap);
	fy_error_check(fyp, sizew == size, err_out,
			"vsnprintf() failed");
	va_end(ap);

	buf[size] = '\0';

	return fy_parser_set_string(fyp, buf);

err_out:
	return -1;
}

static struct fy_document *fy_document_build_internal(const struct fy_parse_cfg *cfg,
		int (*parser_setup)(struct fy_parser *fyp, void *user),
		void *user)
{
	struct fy_parser *fyp = NULL;
	struct fy_document *fyd = NULL;
	struct fy_eventp *fyep;
	bool got_stream_end;
	int rc;

	if (!parser_setup)
		return NULL;

	if (!cfg)
		cfg = &doc_parse_default_cfg;

	fyp = fy_parser_create(cfg);
	if (!fyp)
		return NULL;

	/* no more updating of the document state */
	fyp->external_document_state = true;

	rc = (*parser_setup)(fyp, user);
	fy_error_check(fyp, !rc, err_out,
			"parser_setup() failed");

	fyd = fy_parse_load_document(fyp);

	/* we're going to handle stream errors from now */
	if (!fyd)
		fyp->stream_error = false;

	/* if we collect diagnostics, we can continue */
	fy_error_check(fyp, fyd || (fyp->cfg.flags & FYPCF_COLLECT_DIAG), err_out,
			"fy_parse_load_document() failed");

	/* no document, but we're collecting diagnostics */
	if (!fyd) {

		if (!fyp->stream_error)
			fy_error(fyp, "fy_parse_load_document() failed");
		else
			fy_notice(fyp, "fy_parse_load_document() failed");

		fyp->stream_error = false;
		fyd = fy_parse_document_create(fyp, NULL);
		fy_error_check(fyp, fyd, err_out,
				"fy_parse_document_create() failed");
		fyd->owns_parser = true;
		fyd->parse_error = true;

		fy_parser_move_log_to_document(fyp, fyd);

		return fyd;
	}

	/* move ownership of the parser to the document */
	fyd->owns_parser = true;

	got_stream_end = false;
	while (!got_stream_end && (fyep = fy_parse_private(fyp)) != NULL) {
		if (fyep->e.type == FYET_STREAM_END)
			got_stream_end = true;
		fy_parse_eventp_recycle(fyp, fyep);
	}

	if (got_stream_end) {
		fyep = fy_parse_private(fyp);
		fy_error_check(fyp, !fyep, err_out,
				"more events after stream end");
		fy_parse_eventp_recycle(fyp, fyep);
	}

	return fyd;

err_out:
	fy_document_destroy(fyd);
	fy_parser_destroy(fyp);
	return NULL;
}

struct fy_document *fy_document_build_from_string(const struct fy_parse_cfg *cfg, const char *str)
{
	return fy_document_build_internal(cfg, parser_setup_from_string, (void *)str);
}

struct fy_document *fy_document_build_from_file(const struct fy_parse_cfg *cfg, const char *file)
{
	return fy_document_build_internal(cfg, parser_setup_from_file, (void *)file);
}

struct fy_document *fy_document_build_from_fp(const struct fy_parse_cfg *cfg, FILE *fp)
{
	return fy_document_build_internal(cfg, parser_setup_from_fp, fp);
}

enum fy_node_type fy_node_get_type(struct fy_node *fyn)
{
	/* a NULL is a plain scalar node */
	return fyn ? fyn->type : FYNT_SCALAR;
}

enum fy_node_style fy_node_get_style(struct fy_node *fyn)
{
	/* a NULL is a plain scalar node */
	return fyn ? fyn->style : FYNS_PLAIN;
}

struct fy_node *fy_node_pair_key(struct fy_node_pair *fynp)
{
	return fynp ? fynp->key : NULL;
}

struct fy_node *fy_node_pair_value(struct fy_node_pair *fynp)
{
	return fynp ? fynp->value : NULL;
}

void fy_node_pair_set_key(struct fy_node_pair *fynp, struct fy_node *fyn)
{
	if (!fynp)
		return;
	if (fynp->key)
		fy_node_free(fynp->key);
	fynp->key = fyn;
}

void fy_node_pair_set_value(struct fy_node_pair *fynp, struct fy_node *fyn)
{
	if (!fynp)
		return;
	if (fynp->value)
		fy_node_free(fynp->value);
	fynp->value = fyn;
}

struct fy_node *fy_document_root(struct fy_document *fyd)
{
	return fyd->root;
}

const char *fy_node_get_tag(struct fy_node *fyn, size_t *lenp)
{
	size_t tmplen;

	if (!lenp)
		lenp = &tmplen;

	if (!fyn || !fyn->tag) {
		*lenp = 0;
		return NULL;
	}

	return fy_token_get_text(fyn->tag, lenp);

}

const char *fy_node_get_scalar(struct fy_node *fyn, size_t *lenp)
{
	size_t tmplen;

	if (!lenp)
		lenp = &tmplen;

	if (!fyn || fyn->type != FYNT_SCALAR) {
		*lenp = 0;
		return NULL;
	}

	return fy_token_get_text(fyn->scalar, lenp);
}

const char *fy_node_get_scalar0(struct fy_node *fyn)
{
	if (!fyn || fyn->type != FYNT_SCALAR)
		return NULL;

	return fy_token_get_text0(fyn->scalar);
}

size_t fy_node_get_scalar_length(struct fy_node *fyn)
{

	if (!fyn || fyn->type != FYNT_SCALAR)
		return 0;

	return fy_token_get_text_length(fyn->scalar);
}

struct fy_node *fy_node_sequence_iterate(struct fy_node *fyn, void **prevp)
{
	if (!fyn || fyn->type != FYNT_SEQUENCE || !prevp)
		return NULL;

	return *prevp = *prevp ? fy_node_next(&fyn->sequence, *prevp) : fy_node_list_head(&fyn->sequence);
}

struct fy_node *fy_node_sequence_reverse_iterate(struct fy_node *fyn, void **prevp)
{
	if (!fyn || fyn->type != FYNT_SEQUENCE || !prevp)
		return NULL;

	return *prevp = *prevp ? fy_node_prev(&fyn->sequence, *prevp) : fy_node_list_tail(&fyn->sequence);
}

int fy_node_sequence_item_count(struct fy_node *fyn)
{
	struct fy_node *fyni;
	int count;

	if (!fyn || fyn->type != FYNT_SEQUENCE)
		return 0;

	count = 0;
	for (fyni = fy_node_list_head(&fyn->sequence); fyni; fyni = fy_node_next(&fyn->sequence, fyni))
		count++;
	return count;
}

struct fy_node *fy_node_sequence_get_by_index(struct fy_node *fyn, int index)
{
	struct fy_node *fyni;
	void *iterp = NULL;

	if (!fyn || fyn->type != FYNT_SEQUENCE)
		return NULL;

	if (index >= 0) {
		do {
			fyni = fy_node_sequence_iterate(fyn, &iterp);
		} while (fyni && --index >= 0);
	} else {
		do {
			fyni = fy_node_sequence_reverse_iterate(fyn, &iterp);
		} while (fyni && ++index < 0);
	}

	return fyni;
}

struct fy_node_pair *fy_node_mapping_iterate(struct fy_node *fyn, void **prevp)
{
	if (!fyn || fyn->type != FYNT_MAPPING || !prevp)
		return NULL;

	return *prevp = *prevp ? fy_node_pair_next(&fyn->mapping, *prevp) : fy_node_pair_list_head(&fyn->mapping);
}

struct fy_node_pair *fy_node_mapping_reverse_iterate(struct fy_node *fyn, void **prevp)
{
	if (!fyn || fyn->type != FYNT_MAPPING || !prevp)
		return NULL;

	return *prevp = *prevp ? fy_node_pair_prev(&fyn->mapping, *prevp) : fy_node_pair_list_tail(&fyn->mapping);
}

int fy_node_mapping_item_count(struct fy_node *fyn)
{
	struct fy_node_pair *fynpi;
	int count;

	if (!fyn || fyn->type != FYNT_MAPPING)
		return -1;

	count = 0;
	for (fynpi = fy_node_pair_list_head(&fyn->mapping); fynpi; fynpi = fy_node_pair_next(&fyn->mapping, fynpi))
		count++;
	return count;
}

struct fy_node_pair *fy_node_mapping_get_by_index(struct fy_node *fyn, int index)
{
	struct fy_node_pair *fynpi;
	void *iterp = NULL;

	if (!fyn || fyn->type != FYNT_MAPPING)
		return NULL;

	if (index >= 0) {
		do {
			fynpi = fy_node_mapping_iterate(fyn, &iterp);
		} while (fynpi && --index >= 0);
	} else {
		do {
			fynpi = fy_node_mapping_reverse_iterate(fyn, &iterp);
		} while (fynpi && ++index < 0);
	}

	return fynpi;
}

struct fy_node *fy_node_mapping_lookup_value_by_key(struct fy_node *fyn, struct fy_node *fyn_key)
{
	struct fy_node_pair *fynpi;

	if (!fyn || fyn->type != FYNT_MAPPING)
		return NULL;

	for (fynpi = fy_node_pair_list_head(&fyn->mapping); fynpi;
		fynpi = fy_node_pair_next(&fyn->mapping, fynpi)) {

		if (fy_node_compare(fynpi->key, fyn_key))
			return fynpi->value;
	}

	return NULL;
}

struct fy_node *fy_node_mapping_lookup_by_string(struct fy_node *fyn, const char *key)
{
	struct fy_document *fyd;
	struct fy_node *fyn_value;

	fyd = fy_document_build_from_string(NULL, key);
	if (!fyd)
		return NULL;

	fyn_value = fy_node_mapping_lookup_value_by_key(fyn, fy_document_root(fyd));

	fy_document_destroy(fyd);

	return fyn_value;
}

struct fy_node *fy_node_by_path(struct fy_node *fyn, const char *path)
{
	const char *s, *e;
	char *end_idx;
	char *keybuf, *d;
	char c;
	int idx;

	if (!fyn || !path)
		return NULL;

	/* skip all prefixed / */
	while (*path && *path == '/')
		path++;

	/* for a last component / always match this one */
	if (!*path)
		return fyn;

	/* scalar can't match (it has no key) */
	if (fy_node_is_scalar(fyn))
		return NULL;

	/* for a sequence the only allowed key is [n] where n is the index to follow */
	if (fy_node_is_sequence(fyn)) {
		s = path;
		while (*s && isspace(*s))
			s++;
		if (*s++ != '[')
			return NULL;
		idx = (int)strtol(s, &end_idx, 10);
		s = end_idx;
		while (*s && isspace(*s))
			s++;
		if (*s++ != ']')
			return NULL;
		while (*s && isspace(*s))
			s++;
		path = s;
		return fy_node_by_path(fy_node_sequence_get_by_index(fyn, idx), path);
	}

	/* be a little bit paranoid */
	assert(fy_node_is_mapping(fyn));

	/* scan ahead for the end of the path component
	 * note that we don't do UTF8 here, because all the
	 * escapes are regular ascii characters, i.e.
	 * '/', '*', '&', '.', '{', '}', '[', ']' and '\\'
	 */
	keybuf = alloca(strlen(path) + 1);
	d = keybuf;

	s = path;
	while (*s) {
		c = *s++;
		/* end of path component? */
		if (c == '/')
			break;

		if (c == '\\') {
			/* it must be a valid escape */
			if (!*s || !strchr("/*&.{}[]\\", *s))
				return NULL;
			*d++ = *s++;
		} else if (c == '"') {
			*d++ = '"';
			e = s;
			while (*e && *e != '"') {
				c = *e++;
				if (c == '\\' && *e == '"')
					e++;
			}
			/* not a normal double quote end */
			if (*e != '"')
				return NULL;
			e++;
			memcpy(d, s, e - s);
			d += e - s;
			s = e;
		} else if (c == '\'') {
			*d++ = '\'';
			e = s;
			while (*e && *e != '\'') {
				c = *e++;
				if (c == '\'' && *e == '\'')
					e++;
			}
			/* not a normal single quote end */
			if (*e != '\'')
				return NULL;
			e++;
			memcpy(d, s, e - s);
			d += e - s;
			s = e;
		} else
			*d++ = c;
	}
	/* terminate component */
	*d = '\0';

	/* point path to the next component (or NULL is end */
	path = s;

	return fy_node_by_path(fy_node_mapping_lookup_by_string(fyn, keybuf), path);
}

char *fy_node_get_parent_address(struct fy_node *fyn)
{
	struct fy_node *parent, *fyni;
	struct fy_node_pair *fynp;
	char *path = NULL;
	int idx, ret;

	if (!fyn || !fyn->parent)
		return NULL;

	parent = fyn->parent;

	if (fy_node_is_sequence(parent)) {
		/* for a sequence, find the index */
		idx = 0;
		for (fyni = fy_node_list_head(&parent->sequence); fyni && fyni != fyn;
				fyni = fy_node_next(&parent->sequence, fyni))
			idx++;

		if (fyni) {
			ret = asprintf(&path, "[%d]", idx);
			if (ret == -1)
				path = NULL;
		}

	} else if (fy_node_is_mapping(parent)) {
		idx = 0;
		for (fynp = fy_node_pair_list_head(&parent->mapping); fynp && fynp->value != fyn;
				fynp = fy_node_pair_next(&parent->mapping, fynp))
			idx++;

		if (fynp)
			path = fy_emit_node_to_string(fynp->key, FYECF_MODE_FLOW_ONELINE | FYECF_WIDTH_INF);

	}

	return path;
}

char *fy_node_get_path(struct fy_node *fyn)
{
	struct path_track {
		struct path_track *prev;
		char *path;
	};
	struct path_track *track, *newtrack;
	char *path, *s, *path_mem;
	size_t len;

	if (!fyn)
		return NULL;

	/* easy on the root */
	if (!fyn->parent)
		return strdup("/");

	track = NULL;
	len = 0;
	while ((path = fy_node_get_parent_address(fyn))) {
		newtrack = alloca(sizeof(*newtrack));
		newtrack->prev = track;
		newtrack->path = path;

		track = newtrack;

		len += strlen(path) + 1;

		fyn = fyn->parent;
	}
	len += 2;

	path_mem = malloc(len);

	s = path_mem;

	while (track) {
		len = strlen(track->path);
		if (s) {
			*s++ = '/';
			memcpy(s, track->path, len);
			s += len;
		}
		free(track->path);
		track = track->prev;
	}

	if (s)
		*s = '\0';

	return path_mem;
}

struct fy_node *fy_document_load_node(struct fy_document *fyd)
{
	struct fy_document_state *fyds;
	struct fy_parser *fyp;
	struct fy_eventp *fyep = NULL;
	struct fy_event *fye = NULL;
	struct fy_node *fyn = NULL;
	struct fy_error_ctx ec;
	int rc;
	bool was_stream_start;

	if (!fyd || !fyd->fyp)
		return NULL;

	/* get start document state */
	fyds = fyd->fyds;

	fyp = fyd->fyp;

again:
	was_stream_start = false;
	do {
		/* get next event */
		fyep = fy_parse_private(fyp);

		/* no more */
		if (!fyep)
			return NULL;

		was_stream_start = fyep->e.type == FYET_STREAM_START;

		if (was_stream_start) {
			fy_parse_eventp_recycle(fyp, fyep);
			fyep = NULL;
		}

	} while (was_stream_start);

	fye = &fyep->e;

	/* STREAM_END */
	if (fye->type == FYET_STREAM_END) {
		fy_parse_eventp_recycle(fyp, fyep);

		/* final STREAM_END? */
		if (fyp->state == FYPS_END)
			return NULL;

		/* multi-stream */
		goto again;
	}

	FY_ERROR_CHECK(fyp, fy_document_event_get_token(fye), &ec, FYEM_DOC,
			fye->type == FYET_DOCUMENT_START,
			err_bad_event);

	/* if we have a fixed document state, drop the reference */
	if (fye->document_start.document_state == fyds)
		fy_document_state_unref(fyds);

	fy_doc_debug(fyp, "calling load_node() for root");
	rc = fy_parse_document_load_node(fyp, fyd, fy_parse_private(fyp), &fyn);
	fy_error_check(fyp, !rc, err_out,
			"fy_parse_document_load_node() failed");

	rc = fy_parse_document_load_end(fyp, fyd, fy_parse_private(fyp));
	fy_error_check(fyp, !rc, err_out,
			"fy_parse_document_load_node() failed");

	/* always resolve parents */
	fy_resolve_parent_node(fyd, fyn, NULL);

	return fyn;

err_out:
	fy_parse_eventp_recycle(fyp, fyep);
	return NULL;

err_bad_event:
	fy_error_report(fyp, &ec, "bad event");
	goto err_out;
}

static struct fy_node *fy_node_build_internal(struct fy_document *fyd,
		int (*parser_setup)(struct fy_parser *fyp, void *user),
		void *user)
{
	struct fy_parser *fyp;
	struct fy_node *fyn;
	struct fy_eventp *fyep;
	struct fy_error_ctx ec;
	int rc;
	bool got_stream_end;

	if (!fyd || !fyd->fyp || !parser_setup)
		return NULL;

	fyp = fyd->fyp;

	rc = (*parser_setup)(fyp, user);
	fy_error_check(fyp, !rc, err_out,
			"parser_setup() failed");

	fyn = fy_document_load_node(fyd);
	fy_error_check(fyp, fyn, err_out,
			"fy_document_load_node() failed");

	got_stream_end = false;
	while (!got_stream_end && (fyep = fy_parse_private(fyp)) != NULL) {
		if (fyep->e.type == FYET_STREAM_END)
			got_stream_end = true;
		fy_parse_eventp_recycle(fyp, fyep);
	}

	if (got_stream_end) {
		fyep = fy_parse_private(fyp);

		FY_ERROR_CHECK(fyp, fy_document_event_get_token(&fyep->e), &ec, FYEM_DOC,
				!fyep,
				err_trailing_event);
		fy_parse_eventp_recycle(fyp, fyep);
	}

	return fyn;

err_out:
	return NULL;

err_trailing_event:
	fy_error_report(fyp, &ec, "trailing events after the last");
	goto err_out;
}

struct fy_node *fy_node_build_from_string(struct fy_document *fyd, const char *str)
{
	return fy_node_build_internal(fyd, parser_setup_from_string, (void *)str);
}

struct fy_node *fy_node_build_from_file(struct fy_document *fyd, const char *file)
{
	return fy_node_build_internal(fyd, parser_setup_from_file, (void *)file);
}

struct fy_node *fy_node_build_from_fp(struct fy_document *fyd, FILE *fp)
{
	return fy_node_build_internal(fyd, parser_setup_from_fp, fp);
}

void fy_document_set_root(struct fy_document *fyd, struct fy_node *fyn)
{
	if (!fyd)
		return;

	if (fyd->root) {
		fy_node_free(fyd->root);
		fyd->root = NULL;
	}
	fyn->parent = NULL;
	fyd->root = fyn;
}

struct fy_node *fy_node_create_scalar(struct fy_document *fyd, const char *data, size_t size)
{
	struct fy_parser *fyp;
	struct fy_node *fyn = NULL;
	struct fy_input *fyi;
	struct fy_atom handle;
	enum fy_scalar_style style;

	if (!fyd)
		return NULL;

	if (data && size == (size_t)-1)
		size = strlen(data);

	fyp = fyd->fyp;

	fyn = fy_node_alloc(fyd, FYNT_SCALAR);
	fy_error_check(fyp, fyn, err_out,
			"fy_node_alloc() failed");

	fyi = fy_parse_input_from_data(fyp, data, size, &handle, false);
	fy_error_check(fyp, fyi, err_out,
			"fy_parse_input_from_data() failed");

	style = handle.style == FYAS_PLAIN ? FYSS_PLAIN : FYSS_DOUBLE_QUOTED;

	fyn->scalar = fy_token_create(fyp, FYTT_SCALAR, &handle, style);
	fy_error_check(fyp, fyn->scalar, err_out,
			"fy_token_create() failed");

	return fyn;

err_out:
	fy_node_free(fyn);
	return NULL;
}

struct fy_node *fy_node_create_alias(struct fy_document *fyd, const char *data)
{
	struct fy_parser *fyp;
	struct fy_node *fyn = NULL;
	struct fy_input *fyi;
	struct fy_atom handle;
	size_t size;

	if (!fyd || !data)
		return NULL;

	size = strlen(data);

	fyp = fyd->fyp;

	fyn = fy_node_alloc(fyd, FYNT_SCALAR);
	fy_error_check(fyp, fyn, err_out,
			"fy_node_alloc() failed");

	fyi = fy_parse_input_from_data(fyp, data, size, &handle, false);
	fy_error_check(fyp, fyi, err_out,
			"fy_parse_input_from_data() failed");

	fyn->scalar = fy_token_create(fyp, FYTT_ALIAS, &handle);
	fy_error_check(fyp, fyn->scalar, err_out,
			"fy_token_create() failed");

	fyn->style = FYNS_ALIAS;

	return fyn;

err_out:
	fy_node_free(fyn);
	return NULL;
}

static int tag_handle_length(const char *data, size_t len)
{
	const char *s, *e;
	int c, w;

	s = data;
	e = s + len;

	c = fy_utf8_get(s, e - s, &w);
	if (c != '!')
		return -1;
	s += w;

	c = fy_utf8_get(s, e - s, &w);
	if (fy_is_ws(c))
		return s - data;
	/* if first character is !, empty handle */
	if (c == '!') {
		s += w;
		return s - data;
	}
	if (!fy_is_first_alpha(c))
		return -1;
	s += w;
	while (fy_is_alnum(c = fy_utf8_get(s, e - s, &w)))
		s += w;
	if (c == '!')
		s += w;

	return s - data;
}

static bool tag_uri_is_valid(const char *data, size_t len)
{
	const char *s, *e;
	int w, j, k, width, c;
	uint8_t octet, esc_octets[4];

	s = data;
	e = s + len;

	while ((c = fy_utf8_get(s, e - s, &w)) != -1) {
		if (c != '%') {
			s += w;
			continue;
		}

		width = 0;
		k = 0;
		do {
			/* short URI escape */
			if ((e - s) < 3)
				return false;

			if (width > 0) {
				c = fy_utf8_get(s, e - s, &w);
				if (c != '%')
					return false;
			}

			s += w;

			octet = 0;

			for (j = 0; j < 2; j++) {
				c = fy_utf8_get(s, e - s, &w);
				if (!fy_is_hex(c))
					return false;
				s += w;

				octet <<= 4;
				if (c >= '0' && c <= '9')
					octet |= c - '0';
				else if (c >= 'a' && c <= 'f')
					octet |= 10 + c - 'a';
				else
					octet |= 10 + c - 'A';
			}
			if (!width) {
				width = fy_utf8_width_by_first_octet(octet);

				if (width < 1 || width > 4)
					return false;
				k = 0;
			}
			esc_octets[k++] = octet;

		} while (--width > 0);

		/* now convert to utf8 */
		c = fy_utf8_get(esc_octets, k, &w);

		if (c < 0)
			return false;
	}

	return true;
}

static int tag_uri_length(const char *data, size_t len)
{
	const char *s, *e;
	int c, w, cn, wn, uri_length;

	s = data;
	e = s + len;

	while (fy_is_uri(c = fy_utf8_get(s, e - s, &w))) {
		cn = fy_utf8_get(s + w, e - (s + w), &wn);
		if (fy_is_blankz(cn) && fy_utf8_strchr(",}]", c))
			break;
		s += w;
	}
	uri_length = s - data;

	if (!tag_uri_is_valid(data, uri_length))
		return -1;

	return uri_length;
}


int fy_node_set_tag(struct fy_node *fyn, const char *data, size_t len)
{
	struct fy_document *fyd;
	int total_length, handle_length, uri_length, prefix_length, suffix_length;
	const char *s, *e, *handle_start;
	int c, w, cn, wn;
	struct fy_atom handle;
	struct fy_input *fyi = NULL;
	struct fy_token *fyt = NULL, *fyt_td = NULL;

	if (!fyn || !data || !len || !fyn->fyd)
		return -1;

	fyd = fyn->fyd;

	if (len == (size_t)-1)
		len = strlen(data);

	s = data;
	e = s + len;

	prefix_length = 0;

	/* it must start with '!' */
	c = fy_utf8_get(s, e - s, &w);
	if (c != '!')
		return -1;
	cn = fy_utf8_get(s + w, e - (s + w), &wn);
	if (cn == '<') {
		prefix_length = 2;
		suffix_length = 1;
	} else
		prefix_length = suffix_length = 0;

	if (prefix_length) {
		handle_length = 0; /* set the handle to '' */
		s += prefix_length;
	} else {
		/* either !suffix or !handle!suffix */
		/* we scan back to back, and split handle/suffix */
		handle_length = tag_handle_length(s, e - s);
		if (handle_length <= 0)
			return -1;
		s += handle_length;
	}

	uri_length = tag_uri_length(s, e - s);
	if (uri_length < 0)
		return -1;

	/* a handle? */
	if (!prefix_length && (handle_length == 0 || data[handle_length - 1] != '!')) {
		/* special case, '!', handle set to '' and suffix to '!' */
		if (handle_length == 1 && uri_length == 0) {
			handle_length = 0;
			uri_length = 1;
		} else {
			uri_length = handle_length - 1 + uri_length;
			handle_length = 1;
		}
	}
	total_length = prefix_length + handle_length + uri_length + suffix_length;

	/* everything must be consumed */
	if (total_length != (int)len)
		return -1;

	handle_start = data + prefix_length;

	fyt_td = fy_document_state_lookup_tag_directive(fyd->fyds,
			handle_start, handle_length);
	if (!fyt_td)
		return -1;

	fyi = fy_parse_input_from_data(fyd->fyp, data, len, &handle, true);
	if (!fyi)
		return -1;

	handle.style = FYAS_URI;
	handle.direct_output = false;
	handle.storage_hint = false;

	fyt = fy_token_create(fyd->fyp, FYTT_TAG, &handle, prefix_length,
				handle_length, uri_length, fyt_td);
	if (!fyt)
		return -1;

	fy_token_unref(fyn->tag);
	fyn->tag = fyt;

	return 0;
}

struct fy_node *fy_node_create_sequence(struct fy_document *fyd)
{
	return fy_node_alloc(fyd, FYNT_SEQUENCE);
}

struct fy_node *fy_node_create_mapping(struct fy_document *fyd)
{
	return fy_node_alloc(fyd, FYNT_MAPPING);
}

static int fy_node_sequence_insert_prepare(struct fy_node *fyn_seq, struct fy_node *fyn)
{
	if (!fyn_seq || !fyn || fyn_seq->type != FYNT_SEQUENCE)
		return -1;

	fyn->parent = fyn_seq;
	return 0;
}

int fy_node_sequence_append(struct fy_node *fyn_seq, struct fy_node *fyn)
{
	int ret;

	ret = fy_node_sequence_insert_prepare(fyn_seq, fyn);
	if (ret)
		return ret;

	fy_node_list_add_tail(&fyn_seq->sequence, fyn);
	return 0;
}

int fy_node_sequence_prepend(struct fy_node *fyn_seq, struct fy_node *fyn)
{
	int ret;

	ret = fy_node_sequence_insert_prepare(fyn_seq, fyn);
	if (ret)
		return ret;

	fy_node_list_add(&fyn_seq->sequence, fyn);
	return 0;
}

static bool fy_node_sequence_contains_node(struct fy_node *fyn_seq, struct fy_node *fyn)
{
	struct fy_node *fyni;

	if (!fyn_seq || !fyn || fyn_seq->type != FYNT_SEQUENCE)
		return -1;

	for (fyni = fy_node_list_head(&fyn_seq->sequence); fyni; fyni = fy_node_next(&fyn_seq->sequence, fyni))
		if (fyni == fyn)
			return true;

	return false;
}

int fy_node_sequence_insert_before(struct fy_node *fyn_seq,
				   struct fy_node *fyn_mark, struct fy_node *fyn)
{
	int ret;

	if (!fy_node_sequence_contains_node(fyn_seq, fyn_mark))
		return -1;

	ret = fy_node_sequence_insert_prepare(fyn_seq, fyn);
	if (ret)
		return ret;

	fy_node_list_insert_before(&fyn_seq->sequence, fyn_mark, fyn);

	return 0;
}

int fy_node_sequence_insert_after(struct fy_node *fyn_seq,
				   struct fy_node *fyn_mark, struct fy_node *fyn)
{
	int ret;

	if (!fy_node_sequence_contains_node(fyn_seq, fyn_mark))
		return -1;

	ret = fy_node_sequence_insert_prepare(fyn_seq, fyn);
	if (ret)
		return ret;

	fy_node_list_insert_after(&fyn_seq->sequence, fyn_mark, fyn);

	return 0;
}

struct fy_node *fy_node_sequence_remove(struct fy_node *fyn_seq, struct fy_node *fyn)
{
	if (!fy_node_sequence_contains_node(fyn_seq, fyn))
		return NULL;

	fy_node_list_del(&fyn_seq->sequence, fyn);
	fyn->parent = NULL;
	return fyn;
}

static struct fy_node_pair *
fy_node_mapping_pair_insert_prepare(struct fy_node *fyn_map,
				    struct fy_node *fyn_key, struct fy_node *fyn_value)
{
	struct fy_document *fyd;
	struct fy_node_pair *fynp;

	if (!fyn_map || fyn_map->type != FYNT_MAPPING ||
	    fy_node_mapping_key_is_duplicate(fyn_map, fyn_key))
		return NULL;

	fyd = fyn_map->fyd;
	assert(fyd);

	fynp = fy_node_pair_alloc(fyd);
	if (!fynp)
		return NULL;

	if (fyn_key)
		fyn_key->parent = NULL;
	if (fyn_value)
		fyn_value->parent = fyn_map;

	fynp->key = fyn_key;
	fynp->value = fyn_value;
	fynp->parent = fyn_map;

	return fynp;
}

int fy_node_mapping_append(struct fy_node *fyn_map,
			   struct fy_node *fyn_key, struct fy_node *fyn_value)
{
	struct fy_node_pair *fynp;

	fynp = fy_node_mapping_pair_insert_prepare(fyn_map, fyn_key, fyn_value);
	if (!fynp)
		return -1;

	fy_node_pair_list_add_tail(&fyn_map->mapping, fynp);

	return 0;
}

int fy_node_mapping_prepend(struct fy_node *fyn_map,
			    struct fy_node *fyn_key, struct fy_node *fyn_value)
{
	struct fy_node_pair *fynp;

	fynp = fy_node_mapping_pair_insert_prepare(fyn_map, fyn_key, fyn_value);
	if (!fynp)
		return -1;

	fy_node_pair_list_add(&fyn_map->mapping, fynp);

	return 0;
}

bool fy_node_mapping_contains_pair(struct fy_node *fyn_map, struct fy_node_pair *fynp)
{
	struct fy_node_pair *fynpi;

	if (!fyn_map || !fynp || fyn_map->type != FYNT_MAPPING)
		return -1;

	for (fynpi = fy_node_pair_list_head(&fyn_map->mapping); fynpi; fynpi = fy_node_pair_next(&fyn_map->mapping, fynpi))
		if (fynpi == fynp)
			return true;

	return false;
}

int fy_node_mapping_remove(struct fy_node *fyn_map, struct fy_node_pair *fynp)
{
	if (!fy_node_mapping_contains_pair(fyn_map, fynp))
		return -1;

	fy_node_pair_list_del(&fyn_map->mapping, fynp);

	if (fynp->value)
		fynp->value->parent = NULL;

	fynp->parent = NULL;

	return 0;
}

/* returns value */
struct fy_node *fy_node_mapping_remove_by_key(struct fy_node *fyn_map, struct fy_node *fyn_key)
{
	struct fy_node_pair *fynp;
	struct fy_node *fyn_value;

	fynp = fy_node_mapping_lookup_pair(fyn_map, fyn_key);
	if (!fynp)
		return NULL;

	fyn_value = fynp->value;
	if (fyn_value)
		fyn_value->parent = NULL;

	/* do not free the key if it's the same pointer */
	if (fyn_key != fynp->key)
		fy_node_free(fyn_key);
	fynp->value = NULL;

	fy_node_pair_list_del(&fyn_map->mapping, fynp);

	fy_node_pair_free(fynp);

	return fyn_value;
}

void *fy_node_mapping_sort_ctx_arg(struct fy_node_mapping_sort_ctx *ctx)
{
	return ctx->arg;
}

static int fy_node_mapping_sort_cmp(
#ifdef __APPLE__
void *arg, const void *a, const void *b
#else
const void *a, const void *b, void *arg
#endif
)
{
	struct fy_node_mapping_sort_ctx *ctx = arg;
	struct fy_node_pair * const *fynppa = a, * const *fynppb = b;

	assert(fynppa >= ctx->fynpp && fynppa < ctx->fynpp + ctx->count);
	assert(fynppb >= ctx->fynpp && fynppb < ctx->fynpp + ctx->count);

	return ctx->key_cmp(*fynppa, *fynppb, ctx->arg);
}

/* the default sort method */
static int fy_node_mapping_sort_cmp_default(const struct fy_node_pair *fynp_a,
					    const struct fy_node_pair *fynp_b,
					    void *arg __attribute__((__unused__)))
{
	const char *str_a, *str_b;
	int idx_a, idx_b;
	size_t len_a = 0, len_b = 0, min_len;

	/* order is: maps first, followed by sequences, and last scalars sorted */
	if (!fynp_a->key)
		str_a = "";
	else if (fy_node_is_scalar(fynp_a->key))
		str_a = fy_token_get_text(fynp_a->key->scalar, &len_a);
	else
		str_a = NULL;

	if (!fynp_b->key)
		str_b = "";
	else if (fy_node_is_scalar(fynp_b->key))
		str_b = fy_token_get_text(fynp_b->key->scalar, &len_b);
	else
		str_b = NULL;

	min_len = len_a < len_b ? len_a : len_b;

	/* scalar? perform comparison */
	if (str_a && str_b)
		return strncmp(str_a, str_b, min_len);

	/* b is scalar, a is not */
	if (!str_a && str_b)
		return -1;

	/* a is scalar, b is not */
	if (str_a && !str_b)
		return 1;

	/* different types, mappings win */
	if (fynp_a->key->type != fynp_b->key->type)
		return fynp_a->key->type == FYNT_MAPPING ? -1 : 1;

	/* ok, need to compare indices now */
	idx_a = fy_node_mapping_get_pair_index(fynp_a->parent, fynp_a);
	idx_b = fy_node_mapping_get_pair_index(fynp_b->parent, fynp_b);

	return idx_a > idx_b ? 1 : (idx_a < idx_b ? -1 : 0);
}

void fy_node_mapping_perform_sort(struct fy_node *fyn_map,
		fy_node_mapping_sort_fn key_cmp, void *arg,
		struct fy_node_pair **fynpp, int count)
{
	int i;
	struct fy_node_pair *fynpi;
	struct fy_node_mapping_sort_ctx ctx;

	for (i = 0, fynpi = fy_node_pair_list_head(&fyn_map->mapping); i < count && fynpi;
		fynpi = fy_node_pair_next(&fyn_map->mapping, fynpi), i++)
		fynpp[i] = fynpi;

	/* if there's enough space, put down a NULL at the end */
	if (i < count)
		fynpp[i++] = NULL;
	assert(i == count);

	ctx.key_cmp = key_cmp ? : fy_node_mapping_sort_cmp_default;
	ctx.arg = arg;
	ctx.fynpp = fynpp;
	ctx.count = count;
#ifdef __APPLE__
	qsort_r(fynpp, count, sizeof(*fynpp), &ctx, fy_node_mapping_sort_cmp);
#else
	qsort_r(fynpp, count, sizeof(*fynpp), fy_node_mapping_sort_cmp, &ctx);
#endif
}

struct fy_node_pair **fy_node_mapping_sort_array(struct fy_node *fyn_map,
		fy_node_mapping_sort_fn key_cmp, void *arg, int *countp)
{
	int count;
	struct fy_node_pair **fynpp;

	count = fy_node_mapping_item_count(fyn_map);
	if (count < 0)
		return NULL;

	fynpp = malloc((count + 1) * sizeof(*fynpp));
	if (!fynpp)
		return NULL;

	memset(fynpp, 0, (count + 1) * sizeof(*fynpp));

	fy_node_mapping_perform_sort(fyn_map, key_cmp, arg, fynpp, count);

	if (countp)
		*countp = count;

	return fynpp;
}

void fy_node_mapping_sort_release_array(struct fy_node *fyn_map, struct fy_node_pair **fynpp)
{
	if (!fyn_map || !fynpp)
		return;

	free(fynpp);
}

int fy_node_mapping_sort(struct fy_node *fyn_map,
		fy_node_mapping_sort_fn key_cmp,
		void *arg)
{
	int count, i;
	struct fy_node_pair **fynpp, *fynpi;

	fynpp = fy_node_mapping_sort_array(fyn_map, key_cmp, arg, &count);
	if (!fynpp)
		return -1;

	fy_node_pair_list_init(&fyn_map->mapping);
	for (i = 0; i < count; i++) {
		fynpi = fynpp[i];
		fy_node_pair_list_add_tail(&fyn_map->mapping, fynpi);
	}

	fy_node_mapping_sort_release_array(fyn_map, fynpp);

	return 0;
}

int fy_node_sort(struct fy_node *fyn, fy_node_mapping_sort_fn key_cmp, void *arg)
{
	struct fy_node *fyni;
	struct fy_node_pair *fynp, *fynpi;
	int ret;

	if (!fyn)
		return 0;

	switch (fyn->type) {
	case FYNT_SCALAR:
		break;

	case FYNT_SEQUENCE:
		for (fyni = fy_node_list_head(&fyn->sequence); fyni;
				fyni = fy_node_next(&fyn->sequence, fyni)) {

			fy_node_sort(fyni, key_cmp, arg);
		}
		break;

	case FYNT_MAPPING:
		ret = fy_node_mapping_sort(fyn, key_cmp, arg);
		if (ret)
			return ret;

		for (fynp = fy_node_pair_list_head(&fyn->mapping); fynp; fynp = fynpi) {

			fynpi = fy_node_pair_next(&fyn->mapping, fynp);

			/* the parent of the key is always NULL */
			ret = fy_node_sort(fynp->key, key_cmp, arg);
			if (ret)
				return ret;

			ret = fy_node_sort(fynp->value, key_cmp, arg);
			if (ret)
				return ret;

			fynp->parent = fyn;
		}
		break;
	}

	return 0;
}

int fy_parser_move_log_to_document(struct fy_parser *fyp, struct fy_document *fyd)
{
	size_t nwrite;

	if (!fyp || !fyd)
		return -1;

	if (fyp->errfp)
		fflush(fyp->errfp);

	if (!fyd->errfp) {
		fyd->errfp = open_memstream(&fyd->errbuf, &fyd->errsz);
		if (!fyd->errfp)
			return -1;
	}

	/* copy to the document */
	nwrite = fwrite(fyp->errbuf, 1, fyp->errsz, fyd->errfp);
	if (nwrite != fyp->errsz)
		return -1;

	fflush(fyd->errfp);

	rewind(fyp->errfp);
	fflush(fyp->errfp);

	return 0;
}

bool fy_document_has_error(struct fy_document *fyd)
{
	return fyd->parse_error;
}

const char *fy_document_get_log(struct fy_document *fyd, size_t *sizep)
{
	if (!fyd)
		goto err_no_log;

	if (fyd->errfp)
		fflush(fyd->errfp);

	if (!fyd->errbuf || !fyd->errsz)
		goto err_no_log;

	if (sizep)
		*sizep = fyd->errsz;

	return fyd->errbuf;

err_no_log:
	if (sizep)
		*sizep = 0;
	return NULL;
}

void fy_document_clear_log(struct fy_document *fyd)
{
	if (!fyd)
		return;

	if (fyd->errfp) {
		fclose(fyd->errfp);
		fyd->errfp = NULL;
	}
	if (fyd->errbuf) {
		free(fyd->errbuf);
		fyd->errbuf = NULL;
	}
	fyd->errsz = 0;
	fyd->parse_error = false;
}

struct fy_node *fy_node_vbuildf(struct fy_document *fyd, const char *fmt, va_list ap)
{
	struct fy_document_vbuildf_ctx vctx;
	struct fy_node *fyn;

	vctx.fmt = fmt;
	va_copy(vctx.ap, ap);
	fyn = fy_node_build_internal(fyd, parser_setup_from_fmt_ap, &vctx);
	va_end(ap);

	return fyn;
}

struct fy_node *fy_node_buildf(struct fy_document *fyd, const char *fmt, ...)
{
	struct fy_node *fyn;
	va_list ap;

	va_start(ap, fmt);
	fyn = fy_node_vbuildf(fyd, fmt, ap);
	va_end(ap);

	return fyn;
}

struct fy_document *fy_document_vbuildf(const struct fy_parse_cfg *cfg, const char *fmt, va_list ap)
{
	struct fy_document *fyd;
	struct fy_document_vbuildf_ctx vctx;

	vctx.fmt = fmt;
	va_copy(vctx.ap, ap);
	fyd = fy_document_build_internal(cfg, parser_setup_from_fmt_ap, &vctx);
	va_end(ap);

	return fyd;
}

struct fy_document *fy_document_buildf(const struct fy_parse_cfg *cfg, const char *fmt, ...)
{
	struct fy_document *fyd;
	va_list ap;

	va_start(ap, fmt);
	fyd = fy_document_vbuildf(cfg, fmt, ap);
	va_end(ap);

	return fyd;
}

int fy_node_vscanf(struct fy_node *fyn, const char *fmt, va_list ap)
{
	size_t len;
	char *fmt_cpy, *s, *e, *t, *te, *key, *fmtspec;
	const char *value;
	char *value0;
	size_t value_len, value0_len;
	int count, ret;
	struct fy_node *fynv;
	va_list apt;

	if (!fyn || !fmt)
		goto err_out;

	len = strlen(fmt);
	fmt_cpy = alloca(len + 1);
	memcpy(fmt_cpy, fmt, len + 1);
	s = fmt_cpy;
	e = s + len;

	/* the format is of the form 'access key' %fmt[...] */
	/* so we search for a (non escaped '%) */
	value0 = NULL;
	value0_len = 0;
	count = 0;
	while (s < e) {
		/* a '%' format must exist */
		t = strchr(s, '%');
		if (!t)
			goto err_out;

		/* skip escaped % */
		if (t + 1 < e && t[1] == '%') {
			s = t + 2;
			continue;
		}

		/* trim spaces from key */
		while (isspace(*s))
			s++;
		te = t;
		while (te > s && isspace(te[-1]))
			*--te = '\0';

		key = s;

		/* we have to scan until the next space that's not in char set */
		fmtspec = t;
		while (t < e) {
			if (isspace(*t))
				break;
			/* character set (may include space) */
			if (*t == '[') {
				t++;
				/* skip caret */
				if (t < e && *t == '^')
					t++;
				/* if first character in the set is ']' accept it */
				if (t < e && *t == ']')
					t++;
				/* now skip until end of character set */
				while (t < e && *t != ']')
					t++;
				continue;
			}
			t++;
		}
		if (t < e)
			*t++ = '\0';

		s = t;

		/* find by (relative) path */
		fynv = fy_node_by_path(fyn, key);
		if (!fynv || fynv->type != FYNT_SCALAR)
			break;

		/* there must be a text */
		value = fy_token_get_text(fynv->scalar, &value_len);
		if (!value)
			break;

		/* allocate buffer it's smaller than the one we have already */
		if (!value0 || value0_len < value_len) {
			value0 = alloca(value_len + 1);
			value0_len = value_len;
		}

		memcpy(value0, value, value_len);
		value0[value_len] = '\0';

		va_copy(apt, ap);
		/* scanf, all arguments are pointers */
		(void)va_arg(ap, void *);	/* advance argument pointer */

		/* pass it to the system's scanf method */
		ret = vsscanf(value0, fmtspec, apt);

		/* since it's a single specifier, it must be one on success */
		if (ret != 1)
			break;

		count++;
	}

	return count;

err_out:
	errno = -EINVAL;
	return -1;
}

int fy_node_scanf(struct fy_node *fyn, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = fy_node_vscanf(fyn, fmt, ap);
	va_end(ap);

	return ret;
}

int fy_document_vscanf(struct fy_document *fyd, const char *fmt, va_list ap)
{
	return fy_node_vscanf(fyd->root, fmt, ap);
}

int fy_document_scanf(struct fy_document *fyd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = fy_document_vscanf(fyd, fmt, ap);
	va_end(ap);

	return ret;
}

bool fy_document_has_directives(const struct fy_document *fyd)
{
	struct fy_document_state *fyds;

	if (!fyd)
		return false;

	fyds = fyd->fyds;
	if (!fyds)
		return false;

	return fyds->fyt_vd || !fy_token_list_empty(&fyds->fyt_td);
}

bool fy_document_has_explicit_document_start(const struct fy_document *fyd)
{
	return fyd ? !fyd->fyds->start_implicit : false;
}

bool fy_document_has_explicit_document_end(const struct fy_document *fyd)
{
	return fyd ? !fyd->fyds->end_implicit : false;
}
