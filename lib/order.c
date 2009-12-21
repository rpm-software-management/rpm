/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <popt.h>

#include <rpm/rpmtag.h>
#include <rpm/rpmmacro.h>       /* XXX rpmExpand("%{_dependency_whiteout */
#include <rpm/rpmlog.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>

#include "lib/rpmte_internal.h"	/* XXX tsortInfo_s */
#include "lib/rpmts_internal.h"

#include "debug.h"

/*
 * Strongly Connected Components
 * set of packages (indirectly) requiering each other
 */
struct scc_s {
    int count; /* # of external requires this SCC has */
    /* int qcnt;  # of external requires pointing to this SCC */
    int size;  /* # of members */
    tsortInfo * members;
};

typedef struct scc_s * scc;

struct relation_s {
    tsortInfo   rel_suc;  // pkg requiring this package
    rpmsenseFlags rel_flags; // accumulated flags of the requirements
    struct relation_s * rel_next;
};

typedef struct relation_s * relation;

struct tsortInfo_s {
    rpmte te;
    int	     tsi_count;     // #pkgs this pkg requires
    int	     tsi_qcnt;      // #pkgs requiring this package
    int	     tsi_reqx;       // requires Idx/mark as (queued/loop)
    struct relation_s * tsi_relations;
    struct relation_s * tsi_forward_relations;
    tsortInfo tsi_suc;        // used for queuing (addQ)
    int      tsi_SccIdx;     // # of the SCC the node belongs to
                             // (1 for trivial SCCs)
    int      tsi_SccLowlink; // used for SCC detection
};

struct badDeps_s {
    char * pname;
    const char * qname;
};

static int badDepsInitialized = 0;
static struct badDeps_s * badDeps = NULL;

/**
 */
static void freeBadDeps(void)
{
    if (badDeps) {
	struct badDeps_s * bdp;
	/* bdp->qname is a pointer to pname so doesn't need freeing */
	for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++)
	    bdp->pname = _free(bdp->pname);
	badDeps = _free(badDeps);
    }
    badDepsInitialized = 0;
}

/**
 * Check for dependency relations to be ignored.
 *
 * @param ts		transaction set
 * @param p		successor element (i.e. with Requires: )
 * @param q		predecessor element (i.e. with Provides: )
 * @return		1 if dependency is to be ignored.
 */
static int ignoreDep(const rpmts ts, const rpmte p, const rpmte q)
{
    struct badDeps_s * bdp;

    if (!badDepsInitialized) {
	char * s = rpmExpand("%{?_dependency_whiteout}", NULL);
	const char ** av = NULL;
	int msglvl = (rpmtsFlags(ts) & RPMTRANS_FLAG_DEPLOOPS)
			? RPMLOG_WARNING : RPMLOG_DEBUG;
	int ac = 0;
	int i;

	if (s != NULL && *s != '\0'
	&& !(i = poptParseArgvString(s, &ac, (const char ***)&av))
	&& ac > 0 && av != NULL)
	{
	    bdp = badDeps = xcalloc(ac+1, sizeof(*badDeps));
	    for (i = 0; i < ac; i++, bdp++) {
		char * pname, * qname;

		if (av[i] == NULL)
		    break;
		pname = xstrdup(av[i]);
		if ((qname = strchr(pname, '>')) != NULL)
		    *qname++ = '\0';
		bdp->pname = pname;
		bdp->qname = qname;
		rpmlog(msglvl,
			_("ignore package name relation(s) [%d]\t%s -> %s\n"),
			i, bdp->pname, (bdp->qname ? bdp->qname : "???"));
	    }
	    bdp->pname = NULL;
	    bdp->qname = NULL;
	}
	av = _free(av);
	s = _free(s);
	badDepsInitialized++;
    }

    if (badDeps != NULL)
    for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++) {
	if (rstreq(rpmteN(p), bdp->pname) && rstreq(rpmteN(q), bdp->qname))
	    return 1;
    }
    return 0;
}

static void rpmTSIFree(tsortInfo tsi)
{
    relation rel;

    while (tsi->tsi_relations != NULL) {
	rel = tsi->tsi_relations;
	tsi->tsi_relations = tsi->tsi_relations->rel_next;
	rel = _free(rel);
    }
    while (tsi->tsi_forward_relations != NULL) {
	rel = tsi->tsi_forward_relations;
	tsi->tsi_forward_relations = \
	    tsi->tsi_forward_relations->rel_next;
	rel = _free(rel);
    }
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param requires	relation
 * @return		0 always
 */
static inline int addRelation(rpmts ts,
			      rpmal al,
			      rpmte p,
			      rpmds requires)
{
    rpmte q;
    struct tsortInfo_s * tsi_p, * tsi_q;
    relation rel;
    rpmElementType teType = rpmteType(p);
    rpmsenseFlags flags, dsflags;

    dsflags = rpmdsFlags(requires);

    /* Avoid rpmlib feature and package config dependencies */
    if (dsflags & (RPMSENSE_RPMLIB|RPMSENSE_CONFIG))
	return 0;

    q = rpmalSatisfiesDepend(al, requires);

    /* Avoid deps outside this transaction and self dependencies */
    if (q == NULL || q == p)
	return 0;

    /* Avoid certain dependency relations. */
    if (ignoreDep(ts, p, q))
	return 0;

    /* Erasures are reversed installs. */
    if (teType == TR_REMOVED) {
        rpmte r = p;
        p = q;
        q = r;
	flags = isErasePreReq(dsflags);
    } else {
	flags = isInstallPreReq(dsflags);
    }

    /* map legacy prereq to pre/preun as needed */
    if (isLegacyPreReq(dsflags)) {
	flags |= (teType == TR_ADDED) ?
		 RPMSENSE_SCRIPT_PRE : RPMSENSE_SCRIPT_PREUN;
    }

    tsi_p = rpmteTSI(p);
    tsi_q = rpmteTSI(q);

    /* if relation got already added just update the flags */
    if (tsi_q->tsi_relations && tsi_q->tsi_relations->rel_suc == tsi_p) {
	tsi_q->tsi_relations->rel_flags |= flags;
	tsi_p->tsi_forward_relations->rel_flags |= flags;
	return 0;
    }

    /* Record next "q <- p" relation (i.e. "p" requires "q"). */
    if (p != q) {
	/* bump p predecessor count */
	tsi_p->tsi_count++;
    }

    rel = xcalloc(1, sizeof(*rel));
    rel->rel_suc = tsi_p;
    rel->rel_flags = flags;

    rel->rel_next = tsi_q->tsi_relations;
    tsi_q->tsi_relations = rel;
    if (p != q) {
	/* bump q successor count */
	tsi_q->tsi_qcnt++;
    }

    rel = xcalloc(1, sizeof(*rel));
    rel->rel_suc = tsi_q;
    rel->rel_flags = flags;

    rel->rel_next = tsi_p->tsi_forward_relations;
    tsi_p->tsi_forward_relations = rel;

    return 0;
}

/**
 * Add element to list sorting by tsi_qcnt.
 * @param p		new element
 * @retval qp		address of first element
 * @retval rp		address of last element
 * @param prefcolor
 */
static void addQ(tsortInfo p, tsortInfo * qp, tsortInfo * rp,
		rpm_color_t prefcolor)
{
    tsortInfo q, qprev;

    /* Mark the package as queued. */
    p->tsi_reqx = 1;

    if ((*rp) == NULL) {	/* 1st element */
	/* FIX: double indirection */
	(*rp) = (*qp) = p;
	return;
    }

    /* Find location in queue using metric tsi_qcnt. */
    for (qprev = NULL, q = (*qp);
	 q != NULL;
	 qprev = q, q = q->tsi_suc)
    {
	/* XXX Insure preferred color first. */
	if (rpmteColor(p->te) != prefcolor && rpmteColor(p->te) != rpmteColor(q->te))
	    continue;

	if (q->tsi_qcnt <= p->tsi_qcnt)
	    break;
    }

    if (qprev == NULL) {	/* insert at beginning of list */
	p->tsi_suc = q;
	(*qp) = p;		/* new head */
    } else if (q == NULL) {	/* insert at end of list */
	qprev->tsi_suc = p;
	(*rp) = p;		/* new tail */
    } else {			/* insert between qprev and q */
	p->tsi_suc = q;
	qprev->tsi_suc = p;
    }
}

/* Search for SCCs and return an array last entry has a .size of 0 */
static scc detectSCCs(tsortInfo orderInfo, int nelem, int debugloops)
{
    int index = 0;                /* DFS node number counter */
    tsortInfo stack[nelem];  /* An empty stack of nodes */
    int stackcnt = 0;

    int sccCnt = 2;
    scc SCCs = xcalloc(nelem+3, sizeof(struct scc_s));

    auto void tarjan(tsortInfo tsi);

    void tarjan(tsortInfo tsi) {
	tsortInfo tsi_q;
	relation rel;

        /* use negative index numbers */
	index--;
	/* Set the depth index for p */
	tsi->tsi_SccIdx = index;
	tsi->tsi_SccLowlink = index;

	stack[stackcnt++] = tsi;                   /* Push p on the stack */
	for (rel=tsi->tsi_relations; rel != NULL; rel=rel->rel_next) {
	    /* Consider successors of p */
	    tsi_q = rel->rel_suc;
	    if (tsi_q->tsi_SccIdx > 0)
		/* Ignore already found SCCs */
		continue;
	    if (tsi_q->tsi_SccIdx == 0){
		/* Was successor q not yet visited? */
		tarjan(tsi_q);                       /* Recurse */
		/* negative index numers: use max as it is closer to 0 */
		tsi->tsi_SccLowlink = (
		    tsi->tsi_SccLowlink > tsi_q->tsi_SccLowlink
		    ? tsi->tsi_SccLowlink : tsi_q->tsi_SccLowlink);
	    } else {
		tsi->tsi_SccLowlink = (
		    tsi->tsi_SccLowlink > tsi_q->tsi_SccIdx
		    ? tsi->tsi_SccLowlink : tsi_q->tsi_SccIdx);
	    }
	}

	if (tsi->tsi_SccLowlink == tsi->tsi_SccIdx) {
	    /* v is the root of an SCC? */
	    if (stack[stackcnt-1] == tsi) {
		/* ignore trivial SCCs */
		tsi_q = stack[--stackcnt];
		tsi_q->tsi_SccIdx = 1;
	    } else {
		int stackIdx = stackcnt;
		do {
		    tsi_q = stack[--stackIdx];
		    tsi_q->tsi_SccIdx = sccCnt;
		} while (tsi_q != tsi);

		stackIdx = stackcnt;
		do {
		    tsi_q = stack[--stackIdx];
		    /* Calculate count for the SCC */
		    SCCs[sccCnt].count += tsi_q->tsi_count;
		    /* Subtract internal relations */
		    for (rel=tsi_q->tsi_relations; rel != NULL;
							rel=rel->rel_next) {
			if (rel->rel_suc != tsi_q &&
				rel->rel_suc->tsi_SccIdx == sccCnt)
			    SCCs[sccCnt].count--;
		    }
		} while (tsi_q != tsi);
		SCCs[sccCnt].size = stackcnt - stackIdx;
		/* copy members */
		SCCs[sccCnt].members = xcalloc(SCCs[sccCnt].size,
					       sizeof(tsortInfo));
		memcpy(SCCs[sccCnt].members, stack + stackIdx,
		       SCCs[sccCnt].size * sizeof(tsortInfo));
		stackcnt = stackIdx;
		sccCnt++;
	    }
	}
    }

    for (int i = 0; i < nelem; i++) {
	tsortInfo tsi = &orderInfo[i];
	/* Start a DFS at each node */
	if (tsi->tsi_SccIdx == 0)
	    tarjan(tsi);
    }

    SCCs = xrealloc(SCCs, (sccCnt+1)*sizeof(struct scc_s));
    /* Debug output */
    if (sccCnt > 2) {
	int msglvl = debugloops ?  RPMLOG_WARNING : RPMLOG_DEBUG;
	rpmlog(msglvl, "%i Strongly Connected Components\n", sccCnt-2);
	for (int i = 2; i < sccCnt; i++) {
	    rpmlog(msglvl, "SCC #%i: requires %i packages\n",
		   i, SCCs[i].count);
	    /* loop over members */
	    for (int j = 0; j < SCCs[i].size; j++) {
		tsortInfo member = SCCs[i].members[j];
		rpmlog(msglvl, "\t%s\n", rpmteNEVRA(member->te));
		/* show relations between members */
		relation rel = member->tsi_forward_relations;
		for (; rel != NULL; rel=rel->rel_next) {
		    if (rel->rel_suc->tsi_SccIdx!=i) continue;
		    rpmlog(msglvl, "\t\t%s %s\n",
			   rel->rel_flags ? "=>" : "->",
			   rpmteNEVRA(rel->rel_suc->te));
		}
	    }
	}
    }
    return SCCs;
}

static void collectTE(rpm_color_t prefcolor, tsortInfo q,
		      rpmte * newOrder, int * newOrderCount,
		      scc SCCs,
		      tsortInfo * queue_end,
		      tsortInfo * outer_queue,
		      tsortInfo * outer_queue_end)
{
    char deptypechar = (rpmteType(q->te) == TR_REMOVED ? '-' : '+');

    if (rpmIsDebug()) {
	int depth = 1;
	/* figure depth in tree for nice formatting */
	for (rpmte p = q->te; (p = rpmteParent(p)); depth++) {}
	rpmlog(RPMLOG_DEBUG, "%5d%5d%5d%5d %*s%c%s\n",
	       *newOrderCount, q->tsi_count, q->tsi_qcnt,
	       depth, (2 * depth), "",
	       deptypechar, rpmteNEVRA(q->te));
    }

    newOrder[*newOrderCount] = q->te;
    (*newOrderCount)++;

    /* T6. Erase relations. */
    for (relation rel = q->tsi_relations; rel != NULL; rel = rel->rel_next) {
	tsortInfo p = rel->rel_suc;
	/* ignore already collected packages */
	if (p->tsi_SccIdx == 0) continue;
	if (p == q) continue;

	if (p && (--p->tsi_count) == 0) {
	    (void) rpmteSetParent(p->te, q->te);

	    if (q->tsi_SccIdx > 1 && q->tsi_SccIdx != p->tsi_SccIdx) {
                /* Relation point outside of this SCC: add to outside queue */
		assert(outer_queue != NULL && outer_queue_end != NULL);
		addQ(p, outer_queue, outer_queue_end, prefcolor);
	    } else {
		addQ(p, &q->tsi_suc, queue_end, prefcolor);
	    }
	}
	if (p && p->tsi_SccIdx > 1 &&
	                 p->tsi_SccIdx != q->tsi_SccIdx) {
	    if (--SCCs[p->tsi_SccIdx].count == 0) {
		/* New SCC is ready, add this package as representative */
		(void) rpmteSetParent(p->te, q->te);

		if (outer_queue != NULL) {
		    addQ(p, outer_queue, outer_queue_end, prefcolor);
		} else {
		    addQ(p, &q->tsi_suc, queue_end, prefcolor);
		}
	    }
	}
    }
    q->tsi_SccIdx = 0;
}

static void collectSCC(rpm_color_t prefcolor, tsortInfo p_tsi,
		       rpmte * newOrder, int * newOrderCount,
		       scc SCCs, tsortInfo * queue_end)
{
    int sccNr = p_tsi->tsi_SccIdx;
    struct scc_s SCC = SCCs[sccNr];
    int i;
    int start, end;
    relation rel;

    /* remove p from the outer queue */
    tsortInfo outer_queue_start = p_tsi->tsi_suc;
    p_tsi->tsi_suc = NULL;

    /*
     * Run a multi source Dijkstra's algorithm to find relations
     * that can be zapped with least danger to pre reqs.
     * As weight of the edges is always 1 it is not necessary to
     * sort the vertices by distance as the queue gets them
     * already in order
    */

    /* can use a simple queue as edge weights are always 1 */
    tsortInfo * queue = xmalloc((SCC.size+1) * sizeof(*queue));

    /*
     * Find packages that are prerequired and use them as
     * starting points for the Dijkstra algorithm
     */
    start = end = 0;
    for (i = 0; i < SCC.size; i++) {
	tsortInfo tsi = SCC.members[i];
	tsi->tsi_SccLowlink = INT_MAX;
	for (rel=tsi->tsi_forward_relations; rel != NULL; rel=rel->rel_next) {
	    if (rel->rel_flags && rel->rel_suc->tsi_SccIdx == sccNr) {
		if (rel->rel_suc != tsi) {
		    tsi->tsi_SccLowlink =  0;
		    queue[end++] = tsi;
		} else {
		    tsi->tsi_SccLowlink =  INT_MAX/2;
		}
		break;
	    }
	}
    }

    if (start == end) { /* no regular prereqs; add self prereqs to queue */
	for (i = 0; i < SCC.size; i++) {
	    tsortInfo tsi = SCC.members[i];
	    if (tsi->tsi_SccLowlink != INT_MAX) {
		queue[end++] = tsi;
	    }
	}
    }

    /* Do Dijkstra */
    while (start != end) {
	tsortInfo tsi = queue[start++];
	for (rel=tsi->tsi_forward_relations; rel != NULL; rel=rel->rel_next) {
	    tsortInfo next_tsi = rel->rel_suc;
	    if (next_tsi->tsi_SccIdx != sccNr) continue;
	    if (next_tsi->tsi_SccLowlink > tsi->tsi_SccLowlink+1) {
		next_tsi->tsi_SccLowlink = tsi->tsi_SccLowlink + 1;
		queue[end++] = rel->rel_suc;
	    }
	}
    }
    queue = _free(queue);


    while (1) {
	tsortInfo best = NULL;
	tsortInfo inner_queue_start, inner_queue_end;
	int best_score = 0;

	/* select best candidate to start with */
	for (int i = 0; i < SCC.size; i++) {
	    tsortInfo tsi = SCC.members[i];
	    if (tsi->tsi_SccIdx == 0) /* package already collected */
		continue;
	    if (tsi->tsi_SccLowlink >= best_score) {
		best = tsi;
		best_score = tsi->tsi_SccLowlink;
	    }
	}

	if (best == NULL) /* done */
	    break;

	/* collect best candidate and all packages that get freed */
	inner_queue_start = inner_queue_end = NULL;
	addQ(best, &inner_queue_start, &inner_queue_end, prefcolor);

	for (; inner_queue_start != NULL;
	     inner_queue_start = inner_queue_start->tsi_suc) {
	    /* Mark the package as unqueued. */
	    inner_queue_start->tsi_reqx = 0;
	    collectTE(prefcolor, inner_queue_start, newOrder, newOrderCount,
		      SCCs, &inner_queue_end, &outer_queue_start, queue_end);
	}
    }

    /* restore outer queue */
    p_tsi->tsi_suc = outer_queue_start;
}

int rpmtsOrder(rpmts ts)
{
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpmtsi pi; rpmte p;
    tsortInfo q, r;
    rpmte * newOrder;
    int newOrderCount = 0;
    int rc;
    rpmal erasedPackages = rpmalCreate(5, rpmtsColor(ts), prefcolor);
    scc SCCs;
    int nelem = rpmtsNElements(ts);
    tsortInfo sortInfo = xcalloc(nelem, sizeof(struct tsortInfo_s));

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    /*
     * XXX FIXME: this gets needlesly called twice on normal usage patterns,
     * should track the need for generating the index somewhere
     */
    rpmalMakeIndex(ts->addedPackages);

    /* Create erased package index. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
        rpmalAdd(erasedPackages, p);
    }
    pi = rpmtsiFree(pi);
    rpmalMakeIndex(erasedPackages);

    for (int i = 0; i < nelem; i++) {
	sortInfo[i].te = ts->order[i];
	rpmteSetTSI(ts->order[i], &sortInfo[i]);
    }

    /* Record relations. */
    rpmlog(RPMLOG_DEBUG, "========== recording tsort relations\n");
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	rpmal al = (rpmteType(p) == TR_REMOVED) ? 
		   erasedPackages : ts->addedPackages;
	rpmds requires = rpmdsInit(rpmteDS(p, RPMTAG_REQUIRENAME));

	while (rpmdsNext(requires) >= 0) {
	    /* Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, al, p, requires);
	}
    }
    pi = rpmtsiFree(pi);

    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
    SCCs = detectSCCs(sortInfo, nelem, (rpmtsFlags(ts) & RPMTRANS_FLAG_DEPLOOPS));

    rpmlog(RPMLOG_DEBUG, "========== tsorting packages (order, #predecessors, #succesors, depth)\n");

    for (int i = 0; i < 2; i++) {
	/* Do two separate runs: installs first - then erases */
	int oType = !i ? TR_ADDED : TR_REMOVED;
	q = r = NULL;
	/* Scan for zeroes and add them to the queue */
	for (int e = 0; e < nelem; e++) {
	    tsortInfo p = &sortInfo[e];
	    if (rpmteType(p->te) != oType) continue;
	    if (p->tsi_count != 0)
		continue;
	    p->tsi_suc = NULL;
	    addQ(p, &q, &r, prefcolor);
	}

	/* Add one member of each leaf SCC */
	for (int i = 2; SCCs[i].members != NULL; i++) {
	    tsortInfo member = SCCs[i].members[0];
	    if (SCCs[i].count == 0 && rpmteType(member->te) == oType) {
		addQ(member, &q, &r, prefcolor);
	    }
	}

	while (q != NULL) {
	    /* Mark the package as unqueued. */
	    q->tsi_reqx = 0;
	    if (q->tsi_SccIdx > 1) {
		collectSCC(prefcolor, q, newOrder, &newOrderCount, SCCs, &r);
	    } else {
		collectTE(prefcolor, q, newOrder, &newOrderCount, SCCs, &r,
			  NULL, NULL);
	    }
	    q = q->tsi_suc;
	}
    }

    /* Clean up tsort data */
    for (int i = 0; i < nelem; i++) {
	rpmteSetTSI(ts->order[i], NULL);
	rpmTSIFree(&sortInfo[i]);
    }
    free(sortInfo);

    assert(newOrderCount == ts->orderCount);

    ts->order = _free(ts->order);
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    rc = 0;

    freeBadDeps();

    for (int i = 2; SCCs[i].members != NULL; i++) {
	free(SCCs[i].members);
    }
    free(SCCs);
    rpmalFree(erasedPackages);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    return rc;
}
