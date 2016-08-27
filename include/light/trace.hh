/*  Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __LIGHT_TRACE_H__
#define __LIGHT_TRACE_H__

#include <common/cmdlib.h>
#include <common/mathlib.h>
#include <common/bspfile.h>
#include <common/log.h>
#include <common/threads.h>
#include <common/polylib.h>

#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <limits>
#include <sstream>

enum class hittype_t : uint8_t {
    NONE = 0,
    SOLID = 1,
    SKY = 2
};

const bsp2_dleaf_t *Light_PointInLeaf( const bsp2_t *bsp, const vec3_t point );
int Light_PointContents( const bsp2_t *bsp, const vec3_t point );
void Face_MakeInwardFacingEdgePlanes(const bsp2_t *bsp, const bsp2_dface_t *face, plane_t *out);
int SampleTexture(const bsp2_dface_t *face, const bsp2_t *bsp, const vec3_t point);

/*
 * Convenience functions TestLight and TestSky will test against all shadow
 * casting bmodels and self-shadow the model 'self' if self != NULL. Returns
 * true if sky or light is visible, respectively.
 */
qboolean TestSky(const vec3_t start, const vec3_t dirn, const dmodel_t *self);
qboolean TestLight(const vec3_t start, const vec3_t stop, const dmodel_t *self);
hittype_t DirtTrace(const vec3_t start, const vec3_t dirn, vec_t dist, const dmodel_t *self, vec_t *hitdist_out, plane_t *hitplane_out, const bsp2_dface_t **face_out);

// used for CalcPoints
bool IntersectSingleModel(const vec3_t start, const vec3_t dir, vec_t dist, const dmodel_t *self, vec_t *hitdist_out);

class raystream_t {
public:
    virtual void pushRay(int i, const vec_t *origin, const vec3_t dir, float dist, const dmodel_t *selfshadow, const vec_t *color = nullptr, const vec_t *normalcontrib = nullptr) = 0;
    virtual size_t numPushedRays() = 0;
    virtual void tracePushedRaysOcclusion() = 0;
    virtual void tracePushedRaysIntersection() = 0;
    virtual bool getPushedRayOccluded(size_t j) = 0;
    virtual float getPushedRayDist(size_t j) = 0;
    virtual float getPushedRayHitDist(size_t j) = 0;
    virtual hittype_t getPushedRayHitType(size_t j) = 0;
    virtual void getPushedRayDir(size_t j, vec3_t out) = 0;
    virtual int getPushedRayPointIndex(size_t j) = 0;
    virtual void getPushedRayColor(size_t j, vec3_t out) = 0;
    virtual void getPushedRayNormalContrib(size_t j, vec3_t out) = 0;
    virtual void clearPushedRays() = 0;
    virtual ~raystream_t() {};
};

raystream_t *MakeRayStream(int maxrays);

void MakeTnodes(const bsp2_t *bsp);

#endif /* __LIGHT_TRACE_H__ */