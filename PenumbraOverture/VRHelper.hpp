#pragma once

#include "openvr.h"
#include "math/Math.h"
#include "game/Game.h"
#include "scene/Scene.h"

using namespace hpl;

namespace VRHelper {
  static inline cMatrixf ViveToWorldSpace(cMatrixf viveSpaceMtx, cGame* game) {
    // Subtract Vive-space head position and make relative to world space
    cVector3f headPos_viveSpace = game->vr_head_view_mat.GetTranslation();
    cVector3f headPos_worldSpace = game->GetScene()->GetVRWorldSpaceHeadMatrix().GetTranslation();

    cVector3f handPos_viveSpace = viveSpaceMtx.GetTranslation();

    viveSpaceMtx.SetTranslation(handPos_viveSpace - headPos_viveSpace + headPos_worldSpace);

    return viveSpaceMtx;
  }

  static inline cVector3f CollideCenter(tCollidePointVec collidePoints, int numContactPoints) {
    cVector3f sum;

    for (int i = 0; i < numContactPoints; ++i)
      sum += collidePoints[i].mvPoint;

    return sum / numContactPoints;
  }

  #define EPSILON 0.000001
  #define CROSS(dest,v1,v2) \
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0];
  #define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])
  #define SUB(dest,v1,v2) \
          dest[0]=v1[0]-v2[0]; \
          dest[1]=v1[1]-v2[1]; \
          dest[2]=v1[2]-v2[2]; 

  /* the original jgt code */
  static inline int intersect_triangle(float orig[3], float dir[3],
    float vert0[3], float vert1[3], float vert2[3],
    float *t, float *u, float *v)
  {
    float edge1[3], edge2[3], tvec[3], pvec[3], qvec[3];
    float det, inv_det;

    /* find vectors for two edges sharing vert0 */
    SUB(edge1, vert1, vert0);
    SUB(edge2, vert2, vert0);

    /* begin calculating determinant - also used to calculate U parameter */
    CROSS(pvec, dir, edge2);

    /* if determinant is near zero, ray lies in plane of triangle */
    det = DOT(edge1, pvec);

    if (det > -EPSILON && det < EPSILON)
      return 0;
    inv_det = 1.0 / det;

    /* calculate distance from vert0 to ray origin */
    SUB(tvec, orig, vert0);

    /* calculate U parameter and test bounds */
    *u = DOT(tvec, pvec) * inv_det;
    if (*u < 0.0 || *u > 1.0)
      return 0;

    /* prepare to test V parameter */
    CROSS(qvec, tvec, edge1);

    /* calculate V parameter and test bounds */
    *v = DOT(dir, qvec) * inv_det;
    if (*v < 0.0 || *u + *v > 1.0)
      return 0;

    /* calculate t, ray intersects triangle */
    *t = DOT(edge2, qvec) * inv_det;

    return 1;
  }
}