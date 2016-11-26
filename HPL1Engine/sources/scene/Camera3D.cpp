/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of HPL1 Engine.
 *
 * HPL1 Engine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HPL1 Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HPL1 Engine.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "scene/Camera3D.h"

#include "graphics/LowLevelGraphics.h"
#include "math/Math.h"
#include "scene/Entity3D.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cCamera3D::cCamera3D()
	{
		mvPosition = cVector3f(0);

		mfFOV = cMath::ToRad(70.0f);
		mfAspect = 4.0f/3.0f;
		mfFarClipPlane = 1000.0f;
		mfNearClipPlane = 0.05f;

		mfPitch = 0;
		mfYaw = 0;
		mfRoll = 0;

		mRotateMode = eCameraRotateMode_EulerAngles;
		mMoveMode = eCameraMoveMode_Fly;

		m_mtxView = cMatrixf::Identity;
		m_mtxProjection = cMatrixf::Identity;
		m_mtxMove = cMatrixf::Identity;

		mbViewUpdated = true;
		mbProjectionUpdated = true;
		mbMoveUpdated = true;

		mbInfFarPlane = true;

		mvPitchLimits = cVector2f(kPif/2.0f, -kPif/2.0f);
		mvYawLimits = cVector2f(0,0);

    use_vr_projection_matrix = false;
    use_vr_view_matrix = false;
	}

	//-----------------------------------------------------------------------

	cCamera3D::~cCamera3D()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cCamera3D::SetPosition(const cVector3f &avPos)
	{
		mvPosition = avPos;
		mbViewUpdated = true;

		mNode.SetPosition(mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera3D::SetPitch(float afAngle)
	{ 
		mfPitch = afAngle;
		
		if(mvPitchLimits.x!=0 || mvPitchLimits.y!=0)
		{
			if(mfPitch> mvPitchLimits.x)mfPitch = mvPitchLimits.x;
			if(mfPitch< mvPitchLimits.y)mfPitch = mvPitchLimits.y;
		}

		mbViewUpdated = true; mbMoveUpdated = true;
	}
	void cCamera3D::SetYaw(float afAngle)
	{ 
		mfYaw = afAngle;
		
		if(mvYawLimits.x!=0 || mvYawLimits.y!=0)
		{
			if(mfYaw> mvYawLimits.x)mfYaw = mvYawLimits.x;
			if(mfYaw< mvYawLimits.y)mfYaw = mvYawLimits.y;
		}

		mbViewUpdated = true; mbMoveUpdated = true;
	}
	void cCamera3D::SetRoll(float afAngle)
	{ 
		mfRoll = afAngle;
		mbViewUpdated = true; mbMoveUpdated = true;
	}
	
	//-----------------------------------------------------------------------

	void cCamera3D::AddPitch(float afAngle)
	{
		mfPitch += afAngle;
		
		if(mvPitchLimits.x!=0 || mvPitchLimits.y!=0)
		{
			if(mfPitch> mvPitchLimits.x)mfPitch = mvPitchLimits.x;
			if(mfPitch< mvPitchLimits.y)mfPitch = mvPitchLimits.y;
		}

		mbViewUpdated = true; mbMoveUpdated = true;
	}
	void cCamera3D::AddYaw(float afAngle)
	{ 
		mfYaw += afAngle;
		
		if(mvYawLimits.x!=0 || mvYawLimits.y!=0)
		{
			if(mfYaw> mvYawLimits.x)mfYaw = mvYawLimits.x;
			if(mfYaw< mvYawLimits.y)mfYaw = mvYawLimits.y;
		}
		
		mbViewUpdated = true; mbMoveUpdated = true;
	}
	void cCamera3D::AddRoll(float afAngle)
	{ 
		mfRoll += afAngle;
		mbViewUpdated = true; mbMoveUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera3D::MoveForward(float afDist)
	{
		UpdateMoveMatrix();
		
		mvPosition += m_mtxMove.GetForward()*-afDist;

		mbViewUpdated = true;

		mNode.SetPosition(mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera3D::MoveRight(float afDist)
	{
		UpdateMoveMatrix();

		mvPosition += m_mtxMove.GetRight()*afDist;
		
		mbViewUpdated = true;

		mNode.SetPosition(mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera3D::MoveUp(float afDist)
	{
		UpdateMoveMatrix();

		mvPosition += m_mtxMove.GetUp()*afDist;

		mbViewUpdated = true;

		mNode.SetPosition(mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera3D::SetRotateMode(eCameraRotateMode aMode)
	{
		mRotateMode = aMode;
		mbViewUpdated = true; mbMoveUpdated = true;
	}	
	
	//-----------------------------------------------------------------------

	void cCamera3D::SetMoveMode(eCameraMoveMode aMode)
	{
		mMoveMode = aMode;
		mbMoveUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera3D::ResetRotation()
	{
		mbViewUpdated = false;
		mbMoveUpdated = false;
		m_mtxMove = cMatrixf::Identity;
		m_mtxView = cMatrixf::Identity;

		mfRoll =0;
		mfYaw =0;
		mfPitch =0;
	}

	//-----------------------------------------------------------------------

	cFrustum* cCamera3D::GetFrustum()
	{
		//If the far plane is infinite, still have to use a number on far plane
		//to calculate the near plane in the frustm.
		bool bWasInf=false;
		if(mbInfFarPlane){
			SetInifintiveFarPlane(false);
			bWasInf=true;
		}
		mFrustum.SetViewProjMatrix(GetProjectionMatrix(),
									GetViewMatrix(),
									GetFarClipPlane(),GetNearClipPlane(),
									GetFOV(),GetAspect(),GetPosition(),mbInfFarPlane);
		if(bWasInf){
			SetInifintiveFarPlane(true);
		}

		return &mFrustum;
	}

	//-----------------------------------------------------------------------
  void cCamera3D::SetVRViewMatrix(const cMatrixf& mtx) {
    use_vr_view_matrix = true;
    vr_view_matrix = cMath::MatrixInverse(mtx);
  }

	const cMatrixf& cCamera3D::GetViewMatrix()
	{
    if (use_vr_view_matrix)
      return vr_view_matrix;

		if(mbViewUpdated)
		{
			if(mRotateMode == eCameraRotateMode_EulerAngles)
			{
				m_mtxView=  cMatrixf::Identity;

				m_mtxView = cMath::MatrixMul(cMath::MatrixTranslate(mvPosition *-1), m_mtxView);
				m_mtxView = cMath::MatrixMul(cMath::MatrixRotateY(-mfYaw), m_mtxView);
				m_mtxView = cMath::MatrixMul(cMath::MatrixRotateX(-mfPitch), m_mtxView);
				m_mtxView = cMath::MatrixMul(cMath::MatrixRotateZ(-mfRoll), m_mtxView);
			}

			mbViewUpdated = false;
		}
		return m_mtxView;
	}
	
	//-----------------------------------------------------------------------

  void cCamera3D::SetVRProjectionMatrix(float top, float left, float right, float bottom) {
    use_vr_projection_matrix = true;
    mbProjectionUpdated = true;

    mfvrTop = top;
    mfvrBottom = bottom;
    mfvrRight = right;
    mfvrLeft = left;
  }

  void cCamera3D::SetVRProjectionMatrix(const cMatrixf& mtx) {
    use_vr_projection_matrix = true;
    mbProjectionUpdated = true;
    vr_projection_matrix = mtx;
  }

  void cCamera3D::DisableVRMatrices() {
    use_vr_projection_matrix = false;
    mbProjectionUpdated = true;
  }

	const cMatrixf& cCamera3D::GetProjectionMatrix()
	{
    if (use_vr_projection_matrix) {
      if (mbProjectionUpdated) {
        float fFar = mfFarClipPlane;
        float fNear = mfNearClipPlane;
        float fTop = mfvrTop;
        float fBottom = mfvrBottom;
        float fRight = mfvrRight;
        float fLeft = mfvrLeft;

        float idx = 1.0f / (fRight - fLeft);
        float idy = 1.0f / (fBottom - fTop);
        float idz = 1.0f / (fFar - fNear);
        float sx = fRight + fLeft;
        float sy = fBottom + fTop;

        if (mbInfFarPlane) {
          vr_projection_matrix = cMatrixf(
            2*idx, 0,     sx*idx, 0,
            0,     2*idy, sy*idy, 0,
            0,     0,     -1.0f,  -2.0f * fNear,
            0,     0,     -1.0f,  0);
        }
        else {
          vr_projection_matrix = cMatrixf(
            2*idx, 0,     sx*idx,    0,
            0,     2*idy, sy*idy,    0,
            0,     0,     -fFar*idz, -fFar*fNear*idz,
            0,     0,     -1.0f,     0);
        }

        mbProjectionUpdated = false;
      }

      return vr_projection_matrix;
    }

		if(mbProjectionUpdated)
		{
			float fFar = mfFarClipPlane;
			float fNear = mfNearClipPlane;
			float fTop = tan(mfFOV*0.5f) * fNear;
			float fBottom = -fTop;
			float fRight = mfAspect * fTop;
			float fLeft = mfAspect * fBottom;

			float A = (2.0f*fNear) / (fRight - fLeft);
			float B = (2.0f*fNear) / (fTop - fBottom);
			float D = -1.0f;
			float C,Z;
			if(mbInfFarPlane){
				C= -2.0f * fNear;
				Z= -1.0f;
			}
			else {
				C = -(2.0f*fFar*fNear) / (fFar - fNear);
				Z = -(fFar + fNear)/(fFar - fNear);
			}

			float X = 0;//(fRight + fLeft)/(fRight - fLeft);
			float Y = 0;//(fTop + fBottom)/(fTop - fBottom);


			m_mtxProjection = cMatrixf(
				A,0,X,0,
				0,B,Y,0,
				0,0,Z,C,
				0,0,D,0);

			mbProjectionUpdated = false;
		}
		
		return m_mtxProjection;
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera3D::GetMoveMatrix()
	{
		UpdateMoveMatrix();

		return m_mtxMove;
	}


	//-----------------------------------------------------------------------

	cVector3f cCamera3D::GetEyePosition()
	{
    if (use_vr_view_matrix)
      return cMath::MatrixInverse(vr_view_matrix).GetTranslation();

		return mvPosition;
	}

	//-----------------------------------------------------------------------

	void cCamera3D::SetModelViewMatrix(iLowLevelGraphics* apLowLevel)
	{

	}

	//-----------------------------------------------------------------------

	void cCamera3D::SetProjectionMatrix(iLowLevelGraphics* apLowLevel)
	{

	}
	
	//-----------------------------------------------------------------------

  cVector2f cCamera3D::Project(const cVector3f& avWorldPos, iLowLevelGraphics *apLowLevel)
  {
    cMatrixf vpMatrix = cMath::MatrixMul(GetProjectionMatrix(), GetViewMatrix());

    cVector3f scrPos = cMath::MatrixMul(vpMatrix, avWorldPos);

    float scrX = (scrPos.x + 1.0f) / 2.0f;
    float scrY = (1.0f - scrPos.y) / 2.0f;

    Log("%f, %f\n", scrX, scrY);

    return cVector2f(scrX, scrY);
  }

	cVector3f cCamera3D::UnProject(const cVector2f& avScreenPos, iLowLevelGraphics *apLowLevel)
	{
		//This code uses math::unproejct which is not working currently
		/*cVector3f vPos(avScreenPos.x,avScreenPos.y,-0.1f);
		
		bool bWasInf=false;
		if(mbInfFarPlane){
			SetInifintiveFarPlane(false);bWasInf=true;
		}
		cMatrixf mtxViewProj = cMath::MatrixMul(GetProjectionMatrix(), GetViewMatrix());
		if(bWasInf){
			SetInifintiveFarPlane(true);
		}
		
		cVector2f vScreenSize = apLowLevel->GetVirtualSize();

		return cMath::Vector3UnProject(vPos, cRect2f(0,0,vScreenSize.x, vScreenSize.y), mtxViewProj);*/
		
    if (use_vr_projection_matrix)
      mfFOV = 100.0f;

		cVector2f vScreenSize = apLowLevel->GetVirtualSize();
		cVector2f vNormScreen(	(avScreenPos.x / vScreenSize.x) - 0.5f,
								 0.5f - (avScreenPos.y / vScreenSize.y));
		float afNormalizedSlope = tan(mfFOV*0.5f);

		cVector2f avViewportToWorld;
		avViewportToWorld.y = afNormalizedSlope * mfNearClipPlane * 2.0f;
		avViewportToWorld.x = avViewportToWorld.y * mfAspect;
        
		cVector3f vDir(	vNormScreen.x * avViewportToWorld.x,
						vNormScreen.y * avViewportToWorld.y,
						-mfNearClipPlane);

		cMatrixf mtxRot = cMath::MatrixInverse(GetViewMatrix().GetRotation());

		vDir = cMath::MatrixMul(mtxRot,vDir);
		vDir.Normalise();

		return vDir;
	}

	//-----------------------------------------------------------------------

	void cCamera3D::AttachEntity(iEntity3D *apEntity)
	{
		mNode.AddEntity(apEntity);
	}

	void cCamera3D::RemoveEntity(iEntity3D *apEntity)
	{
		mNode.RemoveEntity(apEntity);
	}

	void cCamera3D::ClearAttachedEntities()
	{
		mNode.ClearEntities();
	}

	//-----------------------------------------------------------------------
	
	cVector3f cCamera3D::GetForward()
	{
		return GetViewMatrix().GetRotation().GetForward()*-1.0f;
	}
	cVector3f cCamera3D::GetRight()
	{
		return GetViewMatrix().GetRotation().GetRight();
	}
	cVector3f cCamera3D::GetUp()
	{
		return GetViewMatrix().GetRotation().GetUp();
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cCamera3D::UpdateMoveMatrix()
	{
    if (use_vr_view_matrix) {
      m_mtxMove = cMath::MatrixInverse(vr_view_matrix.GetRotation());

      return;
    }

		if(mbMoveUpdated)
		{
			if(mRotateMode == eCameraRotateMode_EulerAngles)
			{
				m_mtxMove = cMath::MatrixRotateY(-mfYaw);
				if(mMoveMode==eCameraMoveMode_Fly)
				{
					m_mtxMove = cMath::MatrixMul(cMath::MatrixRotateX(-mfPitch),m_mtxMove);
				}
			}

			mbMoveUpdated = false;
		}
	}

	//-----------------------------------------------------------------------

}
