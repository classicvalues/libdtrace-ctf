/*
 * Copyright 2002 -- 2012 Oracle, Inc.
 *
 * Licensed under the GNU General Public License (GPL), version 2. See the file
 * COPYING in the top level of this tree.
 */

#include <ctf_impl.h>
#include <string.h>

static int
extract_label_info(ctf_file_t *fp, const ctf_lblent_t **ctl, uint_t *num_labels)
{
	const ctf_header_t *h;

	h = (const ctf_header_t *)fp->ctf_data.cts_data;

	/* LINTED - pointer alignment */
	*ctl = (const ctf_lblent_t *)(fp->ctf_buf + h->cth_lbloff);
	*num_labels = (h->cth_objtoff - h->cth_lbloff) / sizeof (ctf_lblent_t);

	return (0);
}

/*
 * Returns the topmost label, or NULL if any errors are encountered
 */
const char *
ctf_label_topmost(ctf_file_t *fp)
{
	const ctf_lblent_t *ctlp = NULL;
	const char *s;
	uint_t num_labels = 0;

	if (extract_label_info(fp, &ctlp, &num_labels) == CTF_ERR)
		return (NULL); /* errno is set */

	if (num_labels == 0) {
		(void) ctf_set_errno(fp, ECTF_NOLABELDATA);
		return (NULL);
	}

	if ((s = ctf_strraw(fp, (ctlp + num_labels - 1)->ctl_label)) == NULL)
		(void) ctf_set_errno(fp, ECTF_CORRUPT);

	return (s);
}

/*
 * Iterate over all labels.  We pass the label string and the lblinfo_t struct
 * to the specified callback function.
 */
int
ctf_label_iter(ctf_file_t *fp, ctf_label_f *func, void *arg)
{
	const ctf_lblent_t *ctlp = NULL;
	uint_t i;
	uint_t num_labels = 0;
	ctf_lblinfo_t linfo;
	const char *lname;
	int rc;

	if (extract_label_info(fp, &ctlp, &num_labels) == CTF_ERR)
		return (CTF_ERR); /* errno is set */

	if (num_labels == 0)
		return (ctf_set_errno(fp, ECTF_NOLABELDATA));

	for (i = 0; i < num_labels; i++, ctlp++) {
		if ((lname = ctf_strraw(fp, ctlp->ctl_label)) == NULL) {
			ctf_dprintf("failed to decode label %u with "
			    "typeidx %u\n", ctlp->ctl_label, ctlp->ctl_typeidx);
			return (ctf_set_errno(fp, ECTF_CORRUPT));
		}

		linfo.ctb_typeidx = ctlp->ctl_typeidx;
		if ((rc = func(lname, &linfo, arg)) != 0)
			return (rc);
	}

	return (0);
}

typedef struct linfo_cb_arg {
	const char *lca_name;    /* Label we want to retrieve info for */
	ctf_lblinfo_t *lca_info; /* Where to store the info about the label */
} linfo_cb_arg_t;

static int
label_info_cb(const char *lname, const ctf_lblinfo_t *linfo, void *arg)
{
	/*
	 * If lname matches the label we are looking for, copy the
	 * lblinfo_t struct for the caller.
	 */
	if (strcmp(lname, ((linfo_cb_arg_t *)arg)->lca_name) == 0) {
		/*
		 * Allow caller not to allocate storage to test if label exists
		 */
		if (((linfo_cb_arg_t *)arg)->lca_info != NULL)
			bcopy(linfo, ((linfo_cb_arg_t *)arg)->lca_info,
			    sizeof (ctf_lblinfo_t));
		return (1); /* Indicate we found a match */
	}

	return (0);
}

/*
 * Retrieve information about the label with name "lname"
 */
int
ctf_label_info(ctf_file_t *fp, const char *lname, ctf_lblinfo_t *linfo)
{
	linfo_cb_arg_t cb_arg;
	int rc;

	cb_arg.lca_name = lname;
	cb_arg.lca_info = linfo;

	if ((rc = ctf_label_iter(fp, label_info_cb, &cb_arg)) == CTF_ERR)
		return (rc);

	if (rc != 1)
		return (ctf_set_errno(fp, ECTF_NOLABEL));

	return (0);
}
