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
#include "openvr.h"

#include "math/Math.h"

#include "scene/Scene.h"
#include "game/Updater.h"
#include "system/LowLevelSystem.h"

#include "graphics/Graphics.h"
#include "resources/Resources.h"
#include "physics/Collider2D.h"
#include "sound/Sound.h"
#include "sound/LowLevelSound.h"
#include "sound/SoundHandler.h"
#include "scene/Camera2D.h"
#include "scene/Camera3D.h"
#include "scene/World2D.h"
#include "scene/World3D.h"
#include "graphics/Renderer2D.h"
#include "graphics/Renderer3D.h"
#include "graphics/RendererPostEffects.h"
#include "graphics/GraphicsDrawer.h"
#include "graphics/RenderList.h"
#include "system/Script.h"
#include "resources/ScriptManager.h"

#include "physics/Physics.h"

#include "resources/FileSearcher.h"
#include "resources/MeshLoaderHandler.h"

#include "GL\GLee.h"

#include "game/Game.h"

namespace hpl {

  extern cGame* gGame;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cScene::cScene(cGraphics *apGraphics,cResources *apResources, cSound* apSound,cPhysics *apPhysics,
					cSystem *apSystem, cAI *apAI, cHaptic *apHaptic)
		: iUpdateable("HPL_Scene")
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpSound = apSound;
		mpPhysics = apPhysics;
		mpSystem = apSystem;
		mpAI = apAI;
		mpHaptic = apHaptic;

		mpCollider2D = hplNew( cCollider2D, () );
		mpCurrentWorld2D = NULL;
		mpCurrentWorld3D = NULL;

		mbCameraIsListener = true;

		mbDrawScene = true;
		mbUpdateMap = true;

		mpActiveCamera = NULL;
	}

	//-----------------------------------------------------------------------

	cScene::~cScene()
	{
		Log("Exiting Scene Module\n");
		Log("--------------------------------------------------------\n");

		STLDeleteAll(mlstWorld3D);
		STLDeleteAll(mlstCamera);

		hplDelete(mpCollider2D);

		Log("--------------------------------------------------------\n\n");

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cCamera2D* cScene::CreateCamera2D(unsigned int alW,unsigned int alH)
	{
		cCamera2D *pCamera =  hplNew( cCamera2D,(alW, alH) );

		//Add Camera to list
		mlstCamera.push_back(pCamera);

		return pCamera;
	}

	//-----------------------------------------------------------------------

	cCamera3D* cScene::CreateCamera3D(eCameraMoveMode aMoveMode)
	{
		cCamera3D *pCamera = hplNew( cCamera3D, () );
		pCamera->SetAspect(mpGraphics->GetLowLevel()->GetScreenSize().x /
							mpGraphics->GetLowLevel()->GetScreenSize().y);

		//Add Camera to list
		mlstCamera.push_back(pCamera);

		return pCamera;
	}


	//-----------------------------------------------------------------------

	void cScene::DestroyCamera(iCamera* apCam)
	{
		for(tCameraListIt it=mlstCamera.begin(); it!=mlstCamera.end();it++)
		{
			if(*it == apCam){
				hplDelete(*it);
				mlstCamera.erase(it);
				break;
			}
		}
	}

	//-----------------------------------------------------------------------
	void cScene::SetCamera(iCamera* pCam)
	{
		mpActiveCamera = pCam;

		if(mbCameraIsListener)
		{
			if(mpActiveCamera->GetType() == eCameraType_2D)
			{
				cCamera2D* pCamera2D = static_cast<cCamera2D*>(mpActiveCamera);

				mpSound->GetLowLevel()->SetListenerPosition(pCamera2D->GetPosition());
			}
		}
	}

	//-----------------------------------------------------------------------

	void cScene::SetCameraPosition(const cVector3f& avPos)
	{
		if(mpActiveCamera->GetType() == eCameraType_2D)
		{
			cCamera2D* pCamera2D = static_cast<cCamera2D*>(mpActiveCamera);

			pCamera2D->SetPosition(avPos);
		}

		if(mbCameraIsListener)
			mpSound->GetLowLevel()->SetListenerPosition(avPos);

	}

	cVector3f cScene::GetCameraPosition()
	{
		if(mpActiveCamera->GetType() == eCameraType_2D)
		{
			cCamera2D* pCamera2D = static_cast<cCamera2D*>(mpActiveCamera);
			return pCamera2D->GetPosition();
		}

		return cVector2f(0);
	}

  cMatrixf cScene::GetVRWorldSpaceHeadMatrix() {
    return GetVRWorldSpaceHeadMatrix(cMatrixf::Identity);
  }

  cMatrixf cScene::GetVRWorldSpaceHeadMatrix(const cMatrixf& offset) {
    if (mpActiveCamera->GetType() == eCameraType_3D) {
      cCamera3D* pCamera3D = static_cast<cCamera3D*>(mpActiveCamera);

      cMatrixf head_mat = gGame->vr_head_view_mat;

      cVector3f pPos = head_mat.GetTranslation();
      pPos.x = pPos.z = 0.0f;

      // This has to be done so you can reach into the floor
      // No longer 1:1 for height, though :c
      pPos.y -= 0.2f;
      pPos.y *= 1.065f;

      head_mat.SetTranslation(pPos);

      head_mat = cMath::MatrixMul(head_mat, offset);

      return cMath::MatrixMul(gGame->vr_player_pos, head_mat);
    }

    return cMatrixf::Identity;
  }

	//-----------------------------------------------------------------------

	cScriptVar* cScene::CreateLocalVar(const tString& asName)
	{
		cScriptVar* pVar;
		pVar= GetLocalVar(asName);
		if(pVar==NULL)
		{
			cScriptVar Var;
			Var.msName = asName;
			m_mapLocalVars.insert(tScriptVarMap::value_type(cString::ToLowerCase(asName),Var));
			pVar= GetLocalVar(asName);
			if(pVar==NULL)FatalError("Very strange error when creating script var!\n");
		}
		return pVar;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cScene::GetLocalVar(const tString& asName)
	{
		tScriptVarMapIt it = m_mapLocalVars.find(cString::ToLowerCase(asName));
		if(it==m_mapLocalVars.end()) return NULL;

		return &it->second;
	}

	//-----------------------------------------------------------------------

	tScriptVarMap* cScene::GetLocalVarMap()
	{
		return &m_mapLocalVars;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cScene::CreateGlobalVar(const tString& asName)
	{
		cScriptVar* pVar;
		pVar= GetGlobalVar(asName);
		if(pVar==NULL)
		{
			cScriptVar Var;
			Var.msName = asName;
			m_mapGlobalVars.insert(tScriptVarMap::value_type(cString::ToLowerCase(asName),Var));
			pVar= GetGlobalVar(asName);
			if(pVar==NULL)FatalError("Very strange error when creating script var!\n");
		}
		return pVar;
	}

	//-----------------------------------------------------------------------

	cScriptVar* cScene::GetGlobalVar(const tString& asName)
	{
		tScriptVarMapIt it = m_mapGlobalVars.find(cString::ToLowerCase(asName));
		if(it==m_mapGlobalVars.end()) return NULL;

		return &it->second;
	}

	//-----------------------------------------------------------------------

	tScriptVarMap* cScene::GetGlobalVarMap()
	{
		return &m_mapGlobalVars;
	}

	//-----------------------------------------------------------------------

	void cScene::UpdateRenderList(float afFrameTime)
	{
		if(mbDrawScene && mpActiveCamera)
		{
			if(mpActiveCamera->GetType() == eCameraType_3D)
			{
				cCamera3D* pCamera3D = static_cast<cCamera3D*>(mpActiveCamera);

				if(mpCurrentWorld3D)
					mpGraphics->GetRenderer3D()->UpdateRenderList(mpCurrentWorld3D, pCamera3D,afFrameTime);
			}
		}
	}

	//-----------------------------------------------------------------------

	void cScene::SetDrawScene(bool abX)
	{
		mbDrawScene = abX;
		mpGraphics->GetRenderer3D()->GetRenderList()->Clear();
	}

	//-----------------------------------------------------------------------

  static inline void LogCompositorError(vr::EVRCompositorError error) {
    if (error != vr::VRCompositorError_None) {
      switch (error) {
      case vr::VRCompositorError_DoNotHaveFocus:
        Log("Error: VRCompositorError_DoNotHaveFocus\n");
        break;

      case vr::VRCompositorError_IncompatibleVersion:
        Log("Error: VRCompositorError_IncompatibleVersion\n");
        break;

      case vr::VRCompositorError_InvalidTexture:
        Log("Error: VRCompositorError_InvalidTexture\n");
        break;

      case vr::VRCompositorError_IsNotSceneApplication:
        Log("Error: VRCompositorError_IsNotSceneApplication\n");
        break;

      case vr::VRCompositorError_SharedTexturesNotSupported:
        Log("Error: VRCompositorError_SharedTexturesNotSupported\n");
        break;

      case vr::VRCompositorError_TextureIsOnWrongDevice:
        Log("Error: VRCompositorError_TextureIsOnWrongDevice\n");
        break;

      case vr::VRCompositorError_TextureUsesUnsupportedFormat:
        Log("Error: VRCompositorError_TextureUsesUnsupportedFormat\n");
        break;

      default:
        Log("Error: Unknown VRCompositorError\n");
        break;
      }
    }
  }

	void cScene::Render(cUpdater* apUpdater, float afFrameTime)
	{
    mpGraphics->GetLowLevel()->SetRenderTarget(nullptr);

    auto hmd = gGame->vr_hmd;

		if(mbDrawScene && mpActiveCamera)
		{
      // When the scene is being rendered in 2D, we want to draw it to the desktop
      // and also to the HMD.
			if(mpActiveCamera->GetType() == eCameraType_2D)
			{
        cCamera2D* pCamera2D = static_cast<cCamera2D*>(mpActiveCamera);

        if (mpCurrentWorld2D) {
          mpCurrentWorld2D->Render(pCamera2D);
        }

        auto llg = mpGraphics->GetLowLevel();

        llg->SetVREnabled(true, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);
        glViewport(0, 0, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);

        // Draw 2D scene for HMD
        for (int i = 0; i < 2; ++i) {
          cCamera3D* pCamera3D = static_cast<cCamera3D*>(mpActiveCamera);

          cMatrixf proj_mat;
          cMatrixf head_mat = gGame->vr_head_view_mat;
          cMatrixf eye_mat;
          cMatrixf view_mat;

          cVector3f forward = cVector3f(0.0f, 0.0f, 0.0f);
          cVector3f up = cVector3f(0.0f, 0.0f, 0.0f);
          cVector3f eyepos = cVector3f(0.0f, 0.0f, 0.0f);

          float top, left, right, bottom;
          if (i == 0) {
            hmd->GetProjectionRaw(vr::Eye_Left, &left, &right, &top, &bottom);
            eye_mat = cMatrixf::FromSteamVRMatrix34(hmd->GetEyeToHeadTransform(vr::Eye_Left));

            glBindFramebuffer(GL_FRAMEBUFFER, mpGraphics->GetRenderer3D()->leftEyeDesc.m_nRenderFramebufferId);
          }
          else {
            hmd->GetProjectionRaw(vr::Eye_Right, &left, &right, &top, &bottom);
            eye_mat = cMatrixf::FromSteamVRMatrix34(hmd->GetEyeToHeadTransform(vr::Eye_Right));

            glBindFramebuffer(GL_FRAMEBUFFER, mpGraphics->GetRenderer3D()->rightEyeDesc.m_nRenderFramebufferId);
          }

          auto worldEyeViewMat = cMath::MatrixMul(head_mat, eye_mat);

          pCamera3D->SetVRViewMatrix(worldEyeViewMat);
          pCamera3D->SetVRProjectionMatrix(top, left, right, bottom);

          llg->SetMatrix(eMatrix_Projection, pCamera3D->GetProjectionMatrix());

          cMatrixf uiViewMat = cMatrixf::Identity;

          // Translate to center of vision
          auto centerTranslationMat = cMath::MatrixTranslate(cVector3f(-400.0f, -200.0f, 0.0f));
          uiViewMat = cMath::MatrixMul(centerTranslationMat, uiViewMat);

          // Scale it down
          auto scaleMat = cMath::MatrixScale(cVector3f(1.0f / 750.0f, -1.0f / 750.0f, 1.0f / 750.0f));
          uiViewMat = cMath::MatrixMul(scaleMat, uiViewMat);

          // Move to face height
          auto faceTranslationMatrix = cMath::MatrixTranslate(cVector3f(0.0f, worldEyeViewMat.GetTranslation().y, 0.0f));
          uiViewMat = cMath::MatrixMul(faceTranslationMatrix, uiViewMat);

          // Project to world space
          uiViewMat = cMath::MatrixMul(cMath::MatrixInverse(worldEyeViewMat), uiViewMat);

          llg->SetMatrix(eMatrix_ModelView, uiViewMat);
          llg->SetDepthTestActive(false);

          glDisable(GL_CULL_FACE);

          mpGraphics->GetRenderer2D()->RenderObjects(pCamera2D, mpCurrentWorld2D->GetGridMapLights(), mpCurrentWorld2D, i == 0, true);

          apUpdater->OnPostSceneDraw();

          mpGraphics->GetDrawer()->DrawAll(i == 0, true);

          glEnable(GL_CULL_FACE);
        }

        llg->SetVREnabled(false);
			}
			else
			{
				cCamera3D* pCamera3D = static_cast<cCamera3D*>(mpActiveCamera);

        auto ingamePos = pCamera3D->GetRealPosition();

				if(mpCurrentWorld3D)
				{
          START_TIMING(RenderWorld)

          auto llg = mpGraphics->GetLowLevel();

          if (gGame->mbRenderToMonitor)
          {
            cMatrixf proj_mat;
            cMatrixf head_mat = gGame->vr_head_view_mat;
            cMatrixf eye_mat;
            cMatrixf view_mat;

            cVector3f forward = cVector3f(0.0f, 0.0f, 0.0f);
            cVector3f up = cVector3f(0.0f, 0.0f, 0.0f);
            cVector3f eyepos = cVector3f(0.0f, 0.0f, 0.0f);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            llg->SetVREnabled(false);
            glViewport(0, 0, llg->GetScreenSize().x, llg->GetScreenSize().y);

            auto worldEyeViewMat = GetVRWorldSpaceHeadMatrix();

            pCamera3D->SetVRViewMatrix(worldEyeViewMat);

            pCamera3D->DisableVRMatrices();

            mpGraphics->GetRenderer3D()->RenderWorld(mpCurrentWorld3D, pCamera3D, afFrameTime);

            apUpdater->OnPostSceneDraw();

            llg->SetMatrix(eMatrix_Projection, pCamera3D->GetProjectionMatrix());

            cMatrixf uiViewMat;

            cMatrixf headInverse = cMath::MatrixInverse(pCamera3D->GetViewMatrix());
            cVector3f uiPos = headInverse.GetTranslation() + pCamera3D->GetViewMatrix().GetRotation().GetForward() * -0.75f;

            auto translateMat = cMath::MatrixTranslate(uiPos);
            auto scaleMat = cMath::MatrixScale(cVector3f(1.0f / 450.0f, -1.0f / 450.0f, 1.0f / 750.0f));

            uiViewMat = cMatrixf::Identity;

            switch (mVRMenuState) {
            case MenuState_Facelock:
            {
              llg->SetDepthTestActive(false);

              // Translate to center of vision
              auto centerTranslationMat = cMath::MatrixTranslate(cVector3f(-400.0f, -250.0f, 0.0f));
              uiViewMat = cMath::MatrixMul(centerTranslationMat, uiViewMat);

              // Scale it down
              uiViewMat = cMath::MatrixMul(scaleMat, uiViewMat);

              // Rotate to face eyes
              uiViewMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), uiViewMat);

              // Translate in front of eyes
              uiViewMat = cMath::MatrixMul(translateMat, uiViewMat);

              mVRMenuModelMatrix = uiViewMat;

              // Project to world space
              uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), uiViewMat);

              break;
            }

            case MenuState_WorldPositionDepthTest:
            {
              llg->SetDepthTestActive(true);

              mVRMenuModelMatrix = mVRMenuTransform;

              // Project to world space
              uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), mVRMenuTransform);
            }

            case MenuState_WorldPosition:
            {
              llg->SetDepthTestActive(false);

              mVRMenuModelMatrix = mVRMenuTransform;

              // Project to world space
              uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), mVRMenuTransform);

              break;
            }
            };

            llg->SetMatrix(eMatrix_ModelView, uiViewMat);

            glDisable(GL_CULL_FACE);

            mpGraphics->GetDrawer()->DrawAll(true, true);

            glEnable(GL_CULL_FACE);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
          }

          for (int i = 0; i < 2; ++i) {
            cMatrixf proj_mat;
            cMatrixf head_mat = gGame->vr_head_view_mat;
            cMatrixf eye_mat;
            cMatrixf view_mat;

            cVector3f forward = cVector3f(0.0f, 0.0f, 0.0f);
            cVector3f up = cVector3f(0.0f, 0.0f, 0.0f);
            cVector3f eyepos = cVector3f(0.0f, 0.0f, 0.0f);

            float top, left, right, bottom;
            if (i == 0) {
              eye_mat = cMatrixf::FromSteamVRMatrix34(hmd->GetEyeToHeadTransform(vr::Eye_Left));

              hmd->GetProjectionRaw(vr::Eye_Left, &left, &right, &top, &bottom);
              pCamera3D->SetVRProjectionMatrix(top, left, right, bottom);

              glBindFramebuffer(GL_FRAMEBUFFER, mpGraphics->GetRenderer3D()->leftEyeDesc.m_nRenderFramebufferId);

              llg->SetVREnabled(true, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);
              glViewport(0, 0, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);
            }
            else if (i == 1) {
              eye_mat = cMatrixf::FromSteamVRMatrix34(hmd->GetEyeToHeadTransform(vr::Eye_Right));

              hmd->GetProjectionRaw(vr::Eye_Right, &left, &right, &top, &bottom);
              pCamera3D->SetVRProjectionMatrix(top, left, right, bottom);

              glBindFramebuffer(GL_FRAMEBUFFER, mpGraphics->GetRenderer3D()->rightEyeDesc.m_nRenderFramebufferId);

              llg->SetVREnabled(true, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);
              glViewport(0, 0, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);
            }

            auto worldEyeViewMat = GetVRWorldSpaceHeadMatrix(eye_mat);

            pCamera3D->SetVRViewMatrix(worldEyeViewMat);

            mpGraphics->GetRenderer3D()->RenderWorld(mpCurrentWorld3D, pCamera3D, (!gGame->mbRenderToMonitor && i == 0) ? afFrameTime : 0);

            apUpdater->OnPostSceneDraw();

            llg->SetMatrix(eMatrix_Projection, pCamera3D->GetProjectionMatrix());

            cMatrixf uiViewMat;

            cMatrixf headInverse = cMath::MatrixInverse(pCamera3D->GetViewMatrix());
            cVector3f uiPos = headInverse.GetTranslation() + pCamera3D->GetViewMatrix().GetRotation().GetForward() * -0.75f;

            auto translateMat = cMath::MatrixTranslate(uiPos);
            auto scaleMat = cMath::MatrixScale(cVector3f(1.0f / 450.0f, -1.0f / 450.0f, 1.0f / 750.0f));

            uiViewMat = cMatrixf::Identity;

            switch (mVRMenuState) {
              case MenuState_Facelock:
              {
                llg->SetDepthTestActive(false);

                // Translate to center of vision
                auto centerTranslationMat = cMath::MatrixTranslate(cVector3f(-400.0f, -250.0f, 0.0f));
                uiViewMat = cMath::MatrixMul(centerTranslationMat, uiViewMat);

                // Scale it down
                uiViewMat = cMath::MatrixMul(scaleMat, uiViewMat);

                // Rotate to face eyes
                uiViewMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), uiViewMat);

                // Translate in front of eyes
                uiViewMat = cMath::MatrixMul(translateMat, uiViewMat);

                mVRMenuModelMatrix = uiViewMat;

                // Project to world space
                uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), uiViewMat);

                break;
              }

              case MenuState_WorldPositionDepthTest:
              {
                llg->SetDepthTestActive(true);

                mVRMenuModelMatrix = mVRMenuTransform;

                // Project to world space
                uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), mVRMenuTransform);
              }

              case MenuState_WorldPosition:
              {
                llg->SetDepthTestActive(false);

                mVRMenuModelMatrix = mVRMenuTransform;

                // Project to world space
                uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), mVRMenuTransform);

                break;
              }
            };
            
            llg->SetMatrix(eMatrix_ModelView, uiViewMat);

            glDisable(GL_CULL_FACE);

            mpGraphics->GetDrawer()->DrawAll(i == 0, true);

            glEnable(GL_CULL_FACE);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
          }

          pCamera3D->SetPosition(ingamePos);

          llg->SetVREnabled(false);

					STOP_TIMING(RenderWorld)
				}

        pCamera3D->SetPosition(ingamePos);
			}
			START_TIMING(PostSceneDraw)
			//apUpdater->OnPostSceneDraw();
			STOP_TIMING(PostSceneDraw)
			
			START_TIMING(PostEffects)
			// mpGraphics->GetRendererPostEffects()->Render();
			STOP_TIMING(PostEffects)
		}
		else
		{
      auto llg = mpGraphics->GetLowLevel();

      glViewport(0, 0, llg->GetScreenSize().x, llg->GetScreenSize().y);

      llg->SetDepthTestActive(false);
      glDisable(GL_CULL_FACE);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      apUpdater->OnPostSceneDraw();

      mpGraphics->GetDrawer()->DrawAll(true, false);

      llg->SetVREnabled(true, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);
      glViewport(0, 0, mpGraphics->GetRenderer3D()->m_nVRRenderWidth, mpGraphics->GetRenderer3D()->m_nVRRenderHeight);

      // Draw 2D scene for HMD
      for (int i = 0; i < 2; ++i) {
        cCamera3D* pCamera3D = static_cast<cCamera3D*>(mpActiveCamera);

        cMatrixf proj_mat;
        cMatrixf head_mat = gGame->vr_head_view_mat;
        cMatrixf eye_mat;
        cMatrixf view_mat;

        cVector3f forward = cVector3f(0.0f, 0.0f, 0.0f);
        cVector3f up = cVector3f(0.0f, 0.0f, 0.0f);
        cVector3f eyepos = cVector3f(0.0f, 0.0f, 0.0f);

        float top, left, right, bottom;
        if (i == 0) {
          hmd->GetProjectionRaw(vr::Eye_Left, &left, &right, &top, &bottom);
          pCamera3D->SetVRProjectionMatrix(top, left, right, bottom);

          eye_mat = cMatrixf::FromSteamVRMatrix34(hmd->GetEyeToHeadTransform(vr::Eye_Left));

          glBindFramebuffer(GL_FRAMEBUFFER, mpGraphics->GetRenderer3D()->leftEyeDesc.m_nRenderFramebufferId);
        }
        else {
          hmd->GetProjectionRaw(vr::Eye_Right, &left, &right, &top, &bottom);
          pCamera3D->SetVRProjectionMatrix(top, left, right, bottom);

          eye_mat = cMatrixf::FromSteamVRMatrix34(hmd->GetEyeToHeadTransform(vr::Eye_Right));

          glBindFramebuffer(GL_FRAMEBUFFER, mpGraphics->GetRenderer3D()->rightEyeDesc.m_nRenderFramebufferId);
        }

        auto worldEyeViewMat = cMath::MatrixMul(head_mat, eye_mat);

        pCamera3D->SetVRViewMatrix(worldEyeViewMat);

        cMatrixf uiViewMat = cMatrixf::Identity;

        // Translate to center of vision
        auto centerTranslationMat = cMath::MatrixTranslate(cVector3f(-400.0f, -200.0f, 0.0f));
        uiViewMat = cMath::MatrixMul(centerTranslationMat, uiViewMat);

        // Scale it down
        auto scaleMat = cMath::MatrixScale(cVector3f(1.0f / 550.0f, -1.0f / 550.0f, 1.0f / 550.0f));
        uiViewMat = cMath::MatrixMul(scaleMat, uiViewMat);

        // Move to face height, edge of play space
        float xSize, ySize;
        vr::VRChaperone()->GetPlayAreaSize(&xSize, &ySize);

        auto faceTranslationMatrix = cMath::MatrixTranslate(cVector3f(0.0f, 1.35f, -ySize / 2.0f + 0.25f));
        uiViewMat = cMath::MatrixMul(faceTranslationMatrix, uiViewMat);

        mVRMainMenuModelMatrix = uiViewMat;

        // Project to world space
        llg->SetIdentityMatrix(eMatrix_Projection);
        llg->SetMatrix(eMatrix_Projection, pCamera3D->GetProjectionMatrix());

        llg->SetIdentityMatrix(eMatrix_ModelView);
        llg->SetMatrix(eMatrix_ModelView, cMath::MatrixMul(cMath::MatrixInverse(worldEyeViewMat), uiViewMat));

        llg->PushMatrix(eMatrix_Projection);
        llg->PushMatrix(eMatrix_ModelView);

        glDisable(GL_CULL_FACE);
        llg->SetDepthTestActive(false);

        apUpdater->OnPostSceneDraw();

        mpGraphics->GetDrawer()->DrawAll(i == 0, true);

        llg->PopMatrix(eMatrix_Projection);
        llg->PopMatrix(eMatrix_ModelView);

        llg->SetIdentityMatrix(eMatrix_Projection);
        llg->SetMatrix(eMatrix_Projection, pCamera3D->GetProjectionMatrix());

        llg->SetIdentityMatrix(eMatrix_ModelView);
        llg->SetMatrix(eMatrix_ModelView, cMath::MatrixMul(cMath::MatrixInverse(worldEyeViewMat), uiViewMat));

        llg->PushMatrix(eMatrix_Projection);
        llg->PushMatrix(eMatrix_ModelView);

        llg->DrawLine(gGame->pointerDrawStart, gGame->pointerDrawEnd, cColor(1.0f, 0.0f, 0.0f, 1.0f));

        llg->PopMatrix(eMatrix_Projection);
        llg->PopMatrix(eMatrix_ModelView);

        glEnable(GL_CULL_FACE);
      }

      llg->SetVREnabled(false);
		}

    {
      vr::Texture_t leftEyeTexture = { (void*)mpGraphics->GetRenderer3D()->leftEyeDesc.m_nRenderTextureId, vr::API_OpenGL, vr::ColorSpace_Gamma };
      vr::EVRCompositorError err = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);

      if (err)
        LogCompositorError(err);
    }

    {
      vr::Texture_t rightEyeTexture = { (void*)mpGraphics->GetRenderer3D()->rightEyeDesc.m_nRenderTextureId, vr::API_OpenGL, vr::ColorSpace_Gamma };
      vr::EVRCompositorError err = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);

      if (err)
        LogCompositorError(err);
    }
		
		apUpdater->OnPostGUIDraw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	//-----------------------------------------------------------------------

	void cScene::Update(float afTimeStep)
	{
		if(mpActiveCamera==NULL)return;

		if(mpActiveCamera->GetType() == eCameraType_2D)
		{
			if(mbUpdateMap && mpCurrentWorld2D)
			{
				mpCurrentWorld2D->Update(afTimeStep);

				if(mpCurrentWorld2D->GetScript())
				{
					mpCurrentWorld2D->GetScript()->Run("OnUpdate()");
				}

				mpGraphics->GetDrawer()->UpdateBackgrounds();
			}
		}
		else
		{
			if(mbCameraIsListener)
			{
				cCamera3D* pCamera3D = static_cast<cCamera3D*>(mpActiveCamera);
				mpSound->GetLowLevel()->SetListenerAttributes(
						pCamera3D->GetPosition(),
						cVector3f(0,0,0),
						pCamera3D->GetForward()*-1.0f,
						pCamera3D->GetUp());
			}

			if(mbUpdateMap && mpCurrentWorld3D)
			{
				mpCurrentWorld3D->Update(afTimeStep);

				if(mpCurrentWorld3D->GetScript())
				{
					mpCurrentWorld3D->GetScript()->Run("OnUpdate()");
				}
			}
		}
	}

	//-----------------------------------------------------------------------

	void cScene::Reset()
	{
		m_mapLocalVars.clear();
		m_mapGlobalVars.clear();
		m_setLoadedMaps.clear();
	}

	//-----------------------------------------------------------------------

	void cScene::RenderWorld2D(cCamera2D *apCam,cWorld2D* apWorld)
	{
		if(apWorld)
		{
			apWorld->Render(apCam);
			mpGraphics->GetRenderer2D()->RenderObjects(apCam,apWorld->GetGridMapLights(),apWorld);
		}
	}

	//-----------------------------------------------------------------------

	cWorld3D* cScene::LoadWorld3D(const tString& asFile, bool abLoadScript, tWorldLoadFlag aFlags)
	{
		//Clear the local script
		m_mapLocalVars.clear();

		///////////////////////////////////
		// Load the map file
		tString asPath = mpResources->GetFileSearcher()->GetFilePath(asFile);
		if(asPath == ""){
			Error("World '%s' doesn't exist\n",asFile.c_str());
			return NULL;
		}

		cWorld3D* pWorld = mpResources->GetMeshLoaderHandler()->LoadWorld(asPath, aFlags);
		if(pWorld==NULL){
			Error("Couldn't load world from '%s'\n",asPath.c_str());
			return NULL;
		}

		////////////////////////////////////////////////////////////
		//Load the script
		iScript* pScript = NULL;
		if(abLoadScript)
		{
			tString sScriptFile = cString::SetFileExt(asFile,"hps");
			pScript = mpResources->GetScriptManager()->CreateScript(sScriptFile);
			if(pScript==NULL){
				Error("Couldn't load script '%s'\n",sScriptFile.c_str());
			}
			else
			{
				pWorld->SetScript(pScript);
			}
		}

		SetWorld3D(pWorld);

		////////////////////////////
		//Add to loaded maps
		tString sName = cString::ToLowerCase(cString::SetFileExt(asFile,""));
		tStringSetIt it = m_setLoadedMaps.find(sName);
		if(it == m_setLoadedMaps.end())
		{
			m_setLoadedMaps.insert(sName);
		}

		////////////////////////////////////////////////////////////
		//Run script start functions
		/*if(pScript)
		{
			//Check if the map has been loaded before, if not run OnStart script.
			tString sName = cString::ToLowerCase(cString::SetFileExt(asFile,""));
			tStringSetIt it = m_setLoadedMaps.find(sName);
			if(it == m_setLoadedMaps.end())
			{
				m_setLoadedMaps.insert(sName);
				pScript->Run("OnStart()");
			}
		}*/

		return pWorld;
	}

	//-----------------------------------------------------------------------

	cWorld3D* cScene::CreateWorld3D(const tString& asName)
	{
		cWorld3D* pWorld = hplNew( cWorld3D, (asName,mpGraphics,mpResources,mpSound,mpPhysics,this,
										mpSystem,mpAI,mpHaptic) );

		mlstWorld3D.push_back(pWorld);

		return pWorld;
	}

	//-----------------------------------------------------------------------

	void cScene::DestroyWorld3D(cWorld3D* apWorld)
	{
		STLFindAndDelete(mlstWorld3D,apWorld);
	}

	//-----------------------------------------------------------------------

	void cScene::SetWorld3D(cWorld3D* apWorld)
	{
		mpCurrentWorld3D = apWorld;

		//Set the world the sound handler uses.
		mpSound->GetSoundHandler()->SetWorld3D(mpCurrentWorld3D);
		//Set the world for physics.
		mpPhysics->SetGameWorld(mpCurrentWorld3D);

	}

	//-----------------------------------------------------------------------

	bool cScene::HasLoadedWorld(const tString &asFile)
	{
		tString sName = cString::ToLowerCase(cString::SetFileExt(asFile,""));
		tStringSetIt it = m_setLoadedMaps.find(sName);

		if(it == m_setLoadedMaps.end())
			return false;
		else
			return true;
	}

	//-----------------------------------------------------------------------

	bool cScene::LoadMap2D(tString asFile)
	{
		mpGraphics->GetDrawer()->ClearBackgrounds();

		cWorld2D *pTempWorld = NULL;
		//temporary save the old map
		if(mpCurrentWorld2D){
			pTempWorld = mpCurrentWorld2D;
		}

		//Clear the local script
		m_mapLocalVars.clear();

		mpCurrentWorld2D = hplNew( cWorld2D, ("",mpGraphics, mpResources,mpSound, mpCollider2D) );

		if(mpCurrentWorld2D->CreateFromFile(asFile)==false)return false;

		if(mpCurrentWorld2D->GetScript())
		{
			//Check if the map has been loaded before, if not run OnStart script.
			tString sName = cString::ToLowerCase(cString::SetFileExt(asFile,""));
			tStringSetIt it = m_setLoadedMaps.find(sName);
			if(it == m_setLoadedMaps.end())
			{
				m_setLoadedMaps.insert(sName);
				mpCurrentWorld2D->GetScript()->Run("OnStart()");
			}

			mpCurrentWorld2D->GetScript()->Run("OnLoad()");
		}

		mpCollider2D->SetWorld(mpCurrentWorld2D);

		if(pTempWorld){
            hplDelete(pTempWorld);
		}

        return true;
	}

	//-----------------------------------------------------------------------
}
