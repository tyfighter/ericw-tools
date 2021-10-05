/*
    Copyright (C) 1996-1997  Id Software, Inc.
    Copyright (C) 1997       Greg Lewis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
// tjunc.c

#include <qbsp/qbsp.hh>

static int numwedges, numwverts;
static int tjuncs;
static int tjuncfaces;

static int cWVerts;
static int cWEdges;

static wvert_t *pWVerts;
static wedge_t *pWEdges;

//============================================================================

#define NUM_HASH 1024

static wedge_t *wedge_hash[NUM_HASH];
static vec3_t hash_min, hash_scale;

static void InitHash(vec3_t mins, vec3_t maxs)
{
    vec3_t size;
    vec_t volume;
    vec_t scale;
    int newsize[2];

    VectorCopy(mins, hash_min);
    VectorSubtract(maxs, mins, size);
    memset(wedge_hash, 0, sizeof(wedge_hash));

    volume = size[0] * size[1];

    scale = sqrt(volume / NUM_HASH);

    newsize[0] = (int)(size[0] / scale);
    newsize[1] = (int)(size[1] / scale);

    hash_scale[0] = newsize[0] / size[0];
    hash_scale[1] = newsize[1] / size[1];
    hash_scale[2] = (vec_t)newsize[1];
}

static unsigned HashVec(vec3_t vec)
{
    unsigned h;

    h = (unsigned)(hash_scale[0] * (vec[0] - hash_min[0]) * hash_scale[2] + hash_scale[1] * (vec[1] - hash_min[1]));
    if (h >= NUM_HASH)
        return NUM_HASH - 1;
    return h;
}

//============================================================================

static void CanonicalVector(const qvec3d &p1, const qvec3d &p2, qvec3d &vec)
{
    VectorSubtract(p2, p1, vec);
    vec_t length = VectorNormalize(vec);
    if (vec[0] > EQUAL_EPSILON)
        return;
    else if (vec[0] < -EQUAL_EPSILON) {
        VectorInverse(vec);
        return;
    } else
        vec[0] = 0;

    if (vec[1] > EQUAL_EPSILON)
        return;
    else if (vec[1] < -EQUAL_EPSILON) {
        VectorInverse(vec);
        return;
    } else
        vec[1] = 0;

    if (vec[2] > EQUAL_EPSILON)
        return;
    else if (vec[2] < -EQUAL_EPSILON) {
        VectorInverse(vec);
        return;
    } else
        vec[2] = 0;

    LogPrint("WARNING: Line {}: Healing degenerate edge ({}) at ({:.3f} {:.3} {:.3})\n", length, p1[0], p1[1], p1[2]);
}

static wedge_t *FindEdge(const qvec3d &p1, const qvec3d &p2, vec_t &t1, vec_t &t2)
{
    qvec3d origin, edgevec;
    wedge_t *edge;
    int h;

    CanonicalVector(p1, p2, edgevec);

    t1 = DotProduct(p1, edgevec);
    t2 = DotProduct(p2, edgevec);

    VectorMA(p1, -t1, edgevec, origin);

    if (t1 > t2) {
        std::swap(t1, t2);
    }

    h = HashVec(&origin[0]);

    for (edge = wedge_hash[h]; edge; edge = edge->next) {
        vec_t temp = edge->origin[0] - origin[0];
        if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
            continue;
        temp = edge->origin[1] - origin[1];
        if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
            continue;
        temp = edge->origin[2] - origin[2];
        if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
            continue;

        temp = edge->dir[0] - edgevec[0];
        if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
            continue;
        temp = edge->dir[1] - edgevec[1];
        if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
            continue;
        temp = edge->dir[2] - edgevec[2];
        if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
            continue;

        return edge;
    }

    if (numwedges >= cWEdges)
        FError("Internal error: didn't allocate enough edges for tjuncs?");
    edge = pWEdges + numwedges;
    numwedges++;

    edge->next = wedge_hash[h];
    wedge_hash[h] = edge;

    VectorCopy(origin, edge->origin);
    VectorCopy(edgevec, edge->dir);
    edge->head.next = edge->head.prev = &edge->head;
    edge->head.t = VECT_MAX;

    return edge;
}

/*
===============
AddVert

===============
*/
static void AddVert(wedge_t *edge, vec_t t)
{
    wvert_t *v, *newv;

    v = edge->head.next;
    do {
        if (fabs(v->t - t) < T_EPSILON)
            return;
        if (v->t > t)
            break;
        v = v->next;
    } while (1);

    // insert a new wvert before v
    if (numwverts >= cWVerts)
        FError("Internal error: didn't allocate enough vertices for tjuncs?");

    newv = pWVerts + numwverts;
    numwverts++;

    newv->t = t;
    newv->next = v;
    newv->prev = v->prev;
    v->prev->next = newv;
    v->prev = newv;
}

/*
===============
AddEdge

===============
*/
static void AddEdge(const qvec3d &p1, const qvec3d &p2)
{
    vec_t t1, t2;
    wedge_t *edge = FindEdge(p1, p2, t1, t2);
    AddVert(edge, t1);
    AddVert(edge, t2);
}

/*
===============
AddFaceEdges

===============
*/
static void AddFaceEdges(face_t *f)
{
    for (size_t i = 0; i < f->w.size(); i++) {
        size_t j = (i + 1) % f->w.size();
        AddEdge(f->w[i], f->w[j]);
    }
}

//============================================================================

/*
 * superface is a large face used as intermediate stage in tjunc fixes,
 * can hold hundreds of edges if needed
 */
#define MAX_SUPERFACE_POINTS 8192

static void SplitFaceForTjunc(face_t *face, face_t *original, face_t **facelist)
{
    winding_t &w = face->w;
    face_t *newf, *chain;
    vec3_t edgevec[2];
    vec_t angle;
    int i, firstcorner, lastcorner;

    chain = NULL;
    do {
        if (w.size() <= MAXPOINTS) {
            /*
             * the face is now small enough without more cutting so
             * copy it back to the original
             */
            *original = *face;
            original->original = chain;
            original->next = *facelist;
            *facelist = original;
            return;
        }

        tjuncfaces++;

restart:
        /* find the last corner */
        VectorSubtract(w[w.size() - 1], w[0], edgevec[0]);
        VectorNormalize(edgevec[0]);
        for (lastcorner = w.size() - 1; lastcorner > 0; lastcorner--) {
            const qvec3d &p0 = w[lastcorner - 1];
            const qvec3d &p1 = w[lastcorner];
            VectorSubtract(p0, p1, edgevec[1]);
            VectorNormalize(edgevec[1]);
            angle = DotProduct(edgevec[0], edgevec[1]);
            if (angle < 1 - ANGLEEPSILON || angle > 1 + ANGLEEPSILON)
                break;
        }

        /* find the first corner */
        VectorSubtract(w[1], w[0], edgevec[0]);
        VectorNormalize(edgevec[0]);
        for (firstcorner = 1; firstcorner < w.size() - 1; firstcorner++) {
            const qvec3d &p0 = w[firstcorner + 1];
            const qvec3d &p1 = w[firstcorner];
            VectorSubtract(p0, p1, edgevec[1]);
            VectorNormalize(edgevec[1]);
            angle = DotProduct(edgevec[0], edgevec[1]);
            if (angle < 1 - ANGLEEPSILON || angle > 1 + ANGLEEPSILON)
                break;
        }

        if (firstcorner + 2 >= MAXPOINTS) {
            /* rotate the point winding */
            vec3_t point0;

            VectorCopy(w[0], point0);
            for (i = 1; i < w.size(); i++)
                VectorCopy(w[i], w[i - 1]);
            VectorCopy(point0, w[w.size() - 1]);
            goto restart;
        }

        /*
         * cut off as big a piece as possible, less than MAXPOINTS, and not
         * past lastcorner
         */
        newf = NewFaceFromFace(face);
        if (face->original)
            FError("original face still exists");

        newf->original = chain;
        chain = newf;
        newf->next = *facelist;
        *facelist = newf;
        if (w.size() - firstcorner <= MAXPOINTS)
            newf->w.resize(firstcorner + 2);
        else if (lastcorner + 2 < MAXPOINTS && w.size() - lastcorner <= MAXPOINTS)
            newf->w.resize(lastcorner + 2);
        else
            newf->w.resize(MAXPOINTS);

        for (i = 0; i < newf->w.size(); i++)
            newf->w[i] = w[i];
        for (i = newf->w.size() - 1; i < w.size(); i++)
            w[i - (newf->w.size() - 2)] = w[i];

        w.resize(w.size() - (newf->w.size() - 2));
    } while (1);
}

/*
===============
FixFaceEdges

===============
*/
static void FixFaceEdges(face_t *face, face_t *superface, face_t **facelist)
{
    int i, j;
    wedge_t *edge;
    wvert_t *v;
    vec_t t1, t2;

    *superface = *face;

restart:
    for (i = 0; i < superface->w.size(); i++) {
        j = (i + 1) % superface->w.size();

        edge = FindEdge(superface->w[i], superface->w[j], t1, t2);

        v = edge->head.next;
        while (v->t < t1 + T_EPSILON)
            v = v->next;

        if (v->t < t2 - T_EPSILON) {
            /* insert a new vertex here */
            if (superface->w.size() == MAX_SUPERFACE_POINTS)
                FError("tjunc fixups generated too many edges (max {})", MAX_SUPERFACE_POINTS);

            tjuncs++;

            // FIXME: a bit of a silly way of handling this
            superface->w.push_back({});

            for (int32_t k = superface->w.size() - 1; k > j; k--)
                VectorCopy(superface->w[k - 1], superface->w[k]);

            vec3_t temp;
            VectorMA(edge->origin, v->t, edge->dir, temp);

            superface->w[j] = temp;
            goto restart;
        }
    }

    if (superface->w.size() <= MAXPOINTS) {
        *face = *superface;
        face->next = *facelist;
        *facelist = face;
        return;
    }

    /* Too many edges - needs to be split into multiple faces */
    SplitFaceForTjunc(superface, face, facelist);
}

//============================================================================

static void tjunc_count_r(node_t *node)
{
    face_t *f;

    if (node->planenum == PLANENUM_LEAF)
        return;

    for (f = node->faces; f; f = f->next)
        cWVerts += f->w.size();

    tjunc_count_r(node->children[0]);
    tjunc_count_r(node->children[1]);
}

static void tjunc_find_r(node_t *node)
{
    face_t *f;

    if (node->planenum == PLANENUM_LEAF)
        return;

    for (f = node->faces; f; f = f->next)
        AddFaceEdges(f);

    tjunc_find_r(node->children[0]);
    tjunc_find_r(node->children[1]);
}

static void tjunc_fix_r(node_t *node, face_t *superface)
{
    face_t *face, *next, *facelist;

    if (node->planenum == PLANENUM_LEAF)
        return;

    facelist = NULL;

    for (face = node->faces; face; face = next) {
        next = face->next;
        FixFaceEdges(face, superface, &facelist);
    }

    node->faces = facelist;

    tjunc_fix_r(node->children[0], superface);
    tjunc_fix_r(node->children[1], superface);
}

/*
===========
tjunc
===========
*/
void TJunc(const mapentity_t *entity, node_t *headnode)
{
    vec3_t maxs, mins;
    int i;

    LogPrint(LOG_PROGRESS, "---- {} ----\n", __func__);

    /*
     * Guess edges = 1/2 verts
     * Verts are arbitrarily multiplied by 2 because there appears to
     * be a need for them to "grow" slightly.
     */
    cWVerts = 0;
    tjunc_count_r(headnode);
    cWEdges = cWVerts;
    cWVerts *= 2;

    pWVerts = new wvert_t[cWVerts]{};
    pWEdges = new wedge_t[cWEdges]{};

    /*
     * identify all points on common edges
     * origin points won't allways be inside the map, so extend the hash area
     */
    for (i = 0; i < 3; i++) {
        if (fabs(entity->bounds.maxs()[i]) > fabs(entity->bounds.mins()[i]))
            maxs[i] = fabs(entity->bounds.maxs()[i]);
        else
            maxs[i] = fabs(entity->bounds.mins()[i]);
    }
    VectorSubtract(vec3_origin, maxs, mins);

    InitHash(mins, maxs);

    numwedges = numwverts = 0;

    tjunc_find_r(headnode);

    LogPrint(LOG_STAT, "     {:8} world edges\n", numwedges);
    LogPrint(LOG_STAT, "     {:8} edge points\n", numwverts);

    face_t superface;

    /* add extra vertexes on edges where needed */
    tjuncs = tjuncfaces = 0;
    tjunc_fix_r(headnode, &superface);

    delete[] pWVerts;
    delete[] pWEdges;

    LogPrint(LOG_STAT, "     {:8} edges added by tjunctions\n", tjuncs);
    LogPrint(LOG_STAT, "     {:8} faces added by tjunctions\n", tjuncfaces);
}
