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
    rpmte * members;
};

typedef struct scc_s * scc;

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
    if (tsi_q->tsi_relations && tsi_q->tsi_relations->rel_suc == p) {
	tsi_q->tsi_relations->rel_flags |= flags;
	tsi_p->tsi_forward_relations->rel_flags |= flags;
	return 0;
    }

    /* Record next "q <- p" relation (i.e. "p" requires "q"). */
    if (p != q) {
	/* bump p predecessor count */
	rpmteTSI(p)->tsi_count++;
    }

    /* Save max. depth in dependency tree */
    if (rpmteDepth(p) <= rpmteDepth(q))
	(void) rpmteSetDepth(p, (rpmteDepth(q) + 1));
    if (rpmteDepth(p) > ts->maxDepth)
	ts->maxDepth = rpmteDepth(p);

    rel = xcalloc(1, sizeof(*rel));
    rel->rel_suc = p;
    rel->rel_flags = flags;

    rel->rel_next = rpmteTSI(q)->tsi_relations;
    rpmteTSI(q)->tsi_relations = rel;
    if (p != q) {
	/* bump q successor count */
	rpmteTSI(q)->tsi_qcnt++;
    }

    rel = xcalloc(1, sizeof(*rel));
    rel->rel_suc = q;
    rel->rel_flags = flags;

    rel->rel_next = rpmteTSI(p)->tsi_forward_relations;
    rpmteTSI(p)->tsi_forward_relations = rel;

    return 0;
}

/**
 * Add element to list sorting by tsi_qcnt.
 * @param p		new element
 * @retval qp		address of first element
 * @retval rp		address of last element
 * @param prefcolor
 */
static void addQ(rpmte p,
		rpmte * qp,
		rpmte * rp,
		rpm_color_t prefcolor)
{
    rpmte q, qprev;

    /* Mark the package as queued. */
    rpmteTSI(p)->tsi_reqx = 1;

    if ((*rp) == NULL) {	/* 1st element */
	/* FIX: double indirection */
	(*rp) = (*qp) = p;
	return;
    }

    /* Find location in queue using metric tsi_qcnt. */
    for (qprev = NULL, q = (*qp);
	 q != NULL;
	 qprev = q, q = rpmteTSI(q)->tsi_suc)
    {
	/* XXX Insure preferred color first. */
	if (rpmteColor(p) != prefcolor && rpmteColor(p) != rpmteColor(q))
	    continue;

	if (rpmteTSI(q)->tsi_qcnt <= rpmteTSI(p)->tsi_qcnt)
	    break;
    }

    if (qprev == NULL) {	/* insert at beginning of list */
	rpmteTSI(p)->tsi_suc = q;
	(*qp) = p;		/* new head */
    } else if (q == NULL) {	/* insert at end of list */
	rpmteTSI(qprev)->tsi_suc = p;
	(*rp) = p;		/* new tail */
    } else {			/* insert between qprev and q */
	rpmteTSI(p)->tsi_suc = q;
	rpmteTSI(qprev)->tsi_suc = p;
    }
}

/* Search for SCCs and return an array last entry has a .size of 0 */
static scc detectSCCs(rpmts ts)
{
    int index = 0;                /* DFS node number counter */
    rpmte stack[ts->orderCount];  /* An empty stack of nodes */
    int stackcnt = 0;
    rpmtsi pi;
    rpmte p;

    int sccCnt = 2;
    scc SCCs = xcalloc(ts->orderCount+3, sizeof(struct scc_s));

    auto void tarjan(rpmte p);

    void tarjan(rpmte p) {
	tsortInfo tsi = rpmteTSI(p);
	rpmte q;
	tsortInfo tsi_q;
	relation rel;

        /* use negative index numbers */
	index--;
	/* Set the depth index for p */
	tsi->tsi_SccIdx = index;
	tsi->tsi_SccLowlink = index;

	stack[stackcnt++] = p;                   /* Push p on the stack */
	for (rel=tsi->tsi_relations; rel != NULL; rel=rel->rel_next) {
	    /* Consider successors of p */
	    q = rel->rel_suc;
	    tsi_q = rpmteTSI(q);
	    if (tsi_q->tsi_SccIdx > 0)
		/* Ignore already found SCCs */
		continue;
	    if (tsi_q->tsi_SccIdx == 0){
		/* Was successor q not yet visited? */
		tarjan(q);                       /* Recurse */
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
	    if (stack[stackcnt-1] == p) {
		/* ignore trivial SCCs */
		q = stack[--stackcnt];
		tsi_q = rpmteTSI(q);
		tsi_q->tsi_SccIdx = 1;
	    } else {
		int stackIdx = stackcnt;
		do {
		    q = stack[--stackIdx];
		    tsi_q = rpmteTSI(q);
		    tsi_q->tsi_SccIdx = sccCnt;
		} while (q != p);

		stackIdx = stackcnt;
		do {
		    q = stack[--stackIdx];
		    tsi_q = rpmteTSI(q);
		    /* Calculate count for the SCC */
		    SCCs[sccCnt].count += tsi_q->tsi_count;
		    /* Subtract internal relations */
		    for (rel=tsi_q->tsi_relations; rel != NULL;
							rel=rel->rel_next) {
			if (rel->rel_suc != q &&
				rpmteTSI(rel->rel_suc)->tsi_SccIdx == sccCnt)
			    SCCs[sccCnt].count--;
		    }
		} while (q != p);
		SCCs[sccCnt].size = stackcnt - stackIdx;
		/* copy members */
		SCCs[sccCnt].members = xcalloc(SCCs[sccCnt].size,
					       sizeof(rpmte));
		memcpy(SCCs[sccCnt].members, stack + stackIdx,
		       SCCs[sccCnt].size * sizeof(rpmte));
		stackcnt = stackIdx;
		sccCnt++;
	    }
	}
    }

    pi = rpmtsiInit(ts);
    while ((p=rpmtsiNext(pi, 0)) != NULL) {
	/* Start a DFS at each node */
	if (rpmteTSI(p)->tsi_SccIdx == 0)
	    tarjan(p);
    }
    pi = rpmtsiFree(pi);

    SCCs = xrealloc(SCCs, (sccCnt+1)*sizeof(struct scc_s));
    /* Debug output */
    if (sccCnt > 2) {
	int msglvl = (rpmtsFlags(ts) & RPMTRANS_FLAG_DEPLOOPS) ?
		     RPMLOG_WARNING : RPMLOG_DEBUG;
	rpmlog(msglvl, "%i Strongly Connected Components\n", sccCnt-2);
	for (int i = 2; i < sccCnt; i++) {
	    rpmlog(msglvl, "SCC #%i: requires %i packages\n",
		   i, SCCs[i].count);
	    /* loop over members */
	    for (int j = 0; j < SCCs[i].size; j++) {
		rpmlog(msglvl, "\t%s\n", rpmteNEVRA(SCCs[i].members[j]));
		/* show relations between members */
		rpmte member = SCCs[i].members[j];
		relation rel = rpmteTSI(member)->tsi_forward_relations;
		for (; rel != NULL; rel=rel->rel_next) {
		    if (rpmteTSI(rel->rel_suc)->tsi_SccIdx!=i) continue;
		    rpmlog(msglvl, "\t\t%s %s\n",
			   rel->rel_flags ? "=>" : "->",
			   rpmteNEVRA(rel->rel_suc));
		}
	    }
	}
    }
    return SCCs;
}

static void collectTE(rpm_color_t prefcolor, rpmte q,
		      rpmte * newOrder, int * newOrderCount,
		      scc SCCs,
		      rpmte * queue_end,
		      rpmte * outer_queue,
		      rpmte * outer_queue_end)
{
    int treex = rpmteTree(q);
    int depth = rpmteDepth(q);
    char deptypechar = (rpmteType(q) == TR_REMOVED ? '-' : '+');
    tsortInfo tsi = rpmteTSI(q);

    rpmlog(RPMLOG_DEBUG, "%5d%5d%5d%5d%5d %*s%c%s\n",
	   *newOrderCount, rpmteNpreds(q),
	   rpmteTSI(q)->tsi_qcnt,
	   treex, depth,
	   (2 * depth), "",
	   deptypechar,
	   (rpmteNEVRA(q) ? rpmteNEVRA(q) : "???"));

    (void) rpmteSetDegree(q, 0);

    newOrder[*newOrderCount] = q;
    (*newOrderCount)++;

    /* T6. Erase relations. */
    for (relation rel = rpmteTSI(q)->tsi_relations;
	 				rel != NULL; rel = rel->rel_next) {
	rpmte p = rel->rel_suc;
	tsortInfo p_tsi = rpmteTSI(p);
	/* ignore already collected packages */
	if (p_tsi->tsi_SccIdx == 0) continue;
	if (p == q) continue;

	if (p && (--p_tsi->tsi_count) == 0) {

	    (void) rpmteSetTree(p, treex);
	    (void) rpmteSetDepth(p, depth+1);
	    (void) rpmteSetParent(p, q);
	    (void) rpmteSetDegree(q, rpmteDegree(q)+1);

	    if (tsi->tsi_SccIdx > 1 && tsi->tsi_SccIdx != p_tsi->tsi_SccIdx) {
                /* Relation point outside of this SCC: add to outside queue */
		assert(outer_queue != NULL && outer_queue_end != NULL);
		addQ(p, outer_queue, outer_queue_end, prefcolor);
	    } else {
		addQ(p, &tsi->tsi_suc, queue_end, prefcolor);
	    }
	}
	if (p && p_tsi->tsi_SccIdx > 1 &&
	                 p_tsi->tsi_SccIdx != rpmteTSI(q)->tsi_SccIdx) {
	    if (--SCCs[p_tsi->tsi_SccIdx].count == 0) {
		/* New SCC is ready, add this package as representative */

		(void) rpmteSetTree(p, treex);
		(void) rpmteSetDepth(p, depth+1);
		(void) rpmteSetParent(p, q);
		(void) rpmteSetDegree(q, rpmteDegree(q)+1);

		if (outer_queue != NULL) {
		    addQ(p, outer_queue, outer_queue_end, prefcolor);
		} else {
		    addQ(p, &rpmteTSI(q)->tsi_suc, queue_end, prefcolor);
		}
	    }
	}
    }
    tsi->tsi_SccIdx = 0;
}

static void collectSCC(rpm_color_t prefcolor, rpmte p,
		       rpmte * newOrder, int * newOrderCount,
		       scc SCCs, rpmte * queue_end)
{
    int sccNr = rpmteTSI(p)->tsi_SccIdx;
    struct scc_s SCC = SCCs[sccNr];
    int i;
    int start, end;
    relation rel;

    /* remove p from the outer queue */
    rpmte outer_queue_start = rpmteTSI(p)->tsi_suc;
    rpmteTSI(p)->tsi_suc = NULL;

    /*
     * Run a multi source Dijkstra's algorithm to find relations
     * that can be zapped with least danger to pre reqs.
     * As weight of the edges is always 1 it is not necessary to
     * sort the vertices by distance as the queue gets them
     * already in order
    */

    /* can use a simple queue as edge weights are always 1 */
    rpmte * queue = xmalloc((SCC.size+1) * sizeof(rpmte));

    /*
     * Find packages that are prerequired and use them as
     * starting points for the Dijkstra algorithm
     */
    start = end = 0;
    for (i = 0; i < SCC.size; i++) {
	rpmte q = SCC.members[i];
	tsortInfo tsi = rpmteTSI(q);
	tsi->tsi_SccLowlink = INT_MAX;
	for (rel=tsi->tsi_forward_relations; rel != NULL; rel=rel->rel_next) {
	    if (rel->rel_flags && rpmteTSI(rel->rel_suc)->tsi_SccIdx == sccNr) {
		if (rel->rel_suc != q) {
		    tsi->tsi_SccLowlink =  0;
		    queue[end++] = q;
		} else {
		    tsi->tsi_SccLowlink =  INT_MAX/2;
		}
		break;
	    }
	}
    }

    if (start == end) { /* no regular prereqs; add self prereqs to queue */
	for (i = 0; i < SCC.size; i++) {
	    rpmte q = SCC.members[i];
	    tsortInfo tsi = rpmteTSI(q);
	    if (tsi->tsi_SccLowlink != INT_MAX) {
		queue[end++] = q;
	    }
	}
    }

    /* Do Dijkstra */
    while (start != end) {
	rpmte q = queue[start++];
	tsortInfo tsi = rpmteTSI(q);
	for (rel=tsi->tsi_forward_relations; rel != NULL; rel=rel->rel_next) {
	    tsortInfo next_tsi = rpmteTSI(rel->rel_suc);
	    if (next_tsi->tsi_SccIdx != sccNr) continue;
	    if (next_tsi->tsi_SccLowlink > tsi->tsi_SccLowlink+1) {
		next_tsi->tsi_SccLowlink = tsi->tsi_SccLowlink + 1;
		queue[end++] = rel->rel_suc;
	    }
	}
    }
    queue = _free(queue);


    while (1) {
	rpmte best = NULL;
	rpmte inner_queue_start, inner_queue_end;
	int best_score = 0;

	/* select best candidate to start with */
	for (int i = 0; i < SCC.size; i++) {
	    rpmte q = SCC.members[i];
	    tsortInfo tsi = rpmteTSI(q);
	    if (tsi->tsi_SccIdx == 0) /* package already collected */
		continue;
	    if (tsi->tsi_SccLowlink >= best_score) {
		best = q;
		best_score = tsi->tsi_SccLowlink;
	    }
	}

	if (best == NULL) /* done */
	    break;

	/* collect best candidate and all packages that get freed */
	inner_queue_start = inner_queue_end = NULL;
	addQ(best, &inner_queue_start, &inner_queue_end, prefcolor);

	for (; inner_queue_start != NULL;
	     inner_queue_start = rpmteTSI(inner_queue_start)->tsi_suc) {
	    /* Mark the package as unqueued. */
	    rpmteTSI(inner_queue_start)->tsi_reqx = 0;
	    collectTE(prefcolor, inner_queue_start, newOrder, newOrderCount,
		      SCCs, &inner_queue_end, &outer_queue_start, queue_end);
	}
    }

    /* restore outer queue */
    rpmteTSI(p)->tsi_suc = outer_queue_start;
}

int rpmtsOrder(rpmts ts)
{
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpmtsi pi; rpmte p;
    rpmte q;
    rpmte r;
    rpmte * newOrder;
    int newOrderCount = 0;
    int treex;
    int rc;
    rpmal erasedPackages = rpmalCreate(5, rpmtsColor(ts), prefcolor);
    scc SCCs;

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

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteNewTSI(p);
    pi = rpmtsiFree(pi);

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

    /* Save predecessor count and mark tree roots. */
    treex = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int npreds = rpmteTSI(p)->tsi_count;

	(void) rpmteSetNpreds(p, npreds);
	(void) rpmteSetDepth(p, 1);

	if (npreds == 0)
	    (void) rpmteSetTree(p, treex++);
	else
	    (void) rpmteSetTree(p, -1);
    }
    pi = rpmtsiFree(pi);
    ts->ntrees = treex;

    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
    SCCs = detectSCCs(ts);

    rpmlog(RPMLOG_DEBUG, "========== tsorting packages (order, #predecessors, #succesors, tree, depth)\n");

    for (int i = 0; i < 2; i++) {
	/* Do two separate runs: installs first - then erases */
	int oType = !i ? TR_ADDED : TR_REMOVED;
	q = r = NULL;
	pi = rpmtsiInit(ts);
	/* Scan for zeroes and add them to the queue */
	while ((p = rpmtsiNext(pi, oType)) != NULL) {
	    if (rpmteTSI(p)->tsi_count != 0)
		continue;
	    rpmteTSI(p)->tsi_suc = NULL;
	    addQ(p, &q, &r, prefcolor);
	}
	pi = rpmtsiFree(pi);

	/* Add one member of each leaf SCC */
	for (int i = 2; SCCs[i].members != NULL; i++) {
	    if (SCCs[i].count == 0 && rpmteType(SCCs[i].members[0]) == oType) {
		p = SCCs[i].members[0];
		rpmteTSI(p)->tsi_suc = NULL;
		addQ(p, &q, &r, prefcolor);
	    }
	}

	/* Process queue */
	for (; q != NULL; q = rpmteTSI(q)->tsi_suc) {
	    /* Mark the package as unqueued. */
	    rpmteTSI(q)->tsi_reqx = 0;
	    if (rpmteTSI(q)->tsi_SccIdx > 1) {
		collectSCC(prefcolor, q, newOrder, &newOrderCount, SCCs, &r);
	    } else {
		collectTE(prefcolor, q, newOrder, &newOrderCount, SCCs, &r,
			  NULL, NULL);
	    }
	}
    }

    /* Clean up tsort data */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteFreeTSI(p);
    pi = rpmtsiFree(pi);

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
