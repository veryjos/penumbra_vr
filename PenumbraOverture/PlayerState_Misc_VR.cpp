/*
* Copyright (C) 2006-2010 - Frictional Games
*
* This file is part of Penumbra Overture.
*
* Penumbra Overture is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Penumbra Overture is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Penumbra Overture.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "StdAfx.h"
#include "PlayerState_Misc_VR.h"

#include "Init.h"
#include "Player.h"
#include "EffectHandler.h"
#include "GameLadder.h"
#include "GameArea.h"

#include "VRHelper.hpp"


//////////////////////////////////////////////////////////////////////////
// NORMAL STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_Normal_VR::cPlayerState_Normal_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_Normal)
{
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnUpdate(float afTimeStep)
{
  if (mpInit->mpNotebook->IsActive() == false &&
    mpInit->mpInventory->IsActive() == false &&
    mpInit->mpNumericalPanel->IsActive() == false &&
    mpInit->mpDeathMenu->IsActive() == false)
  {
    mpPlayer->ResetCrossHairPos();
  }

  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  /////////////////////////////////////
  // If run is down, run!!
  cInput *pInput = mpInit->mpGame->GetInput();
  if (pInput->IsTriggerd("Run") &&
    mpPlayer->GetMoveState() == ePlayerMoveState_Walk)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
  }

  cVector3f handSize(40.0f, 15.0f, 23.5f);
  auto collider = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld()->CreateBoxShape(handSize, NULL);

  struct touchedObject {
    iPhysicsBody* physicsBody;
    float planeDistance;
    eCrossHairState crosshair;
    cCollideData collideData;
  };

  {
    cMatrixf poseMatrix;

    for (int i = 0; i < 2; ++i) {
      mpPlayer->SetHandCrossHairState(i, eCrossHairState_None);

      if (mpInit->mpPlayerHands->GetCurrentModel(i) &&
          mpInit->mpPlayerHands->GetCurrentModel(i)->msName == "Hand") {

        auto hand = mpInit->mpPlayerHands->GetCurrentModel(i);
        hand->UpdatePoseMatrix(poseMatrix, 0.0f);

        cVector3f handCenter = poseMatrix.GetTranslation();

        auto mtxTouch = poseMatrix.GetRotation();
        mtxTouch = cMath::MatrixMul(cMath::MatrixTranslate(handCenter), mtxTouch);

        // Area body should take priority over everything else
        bool useTraceResults = false;

        {
          // Using area requires line-of-sight
          cVector3f vStart, vEnd;

          vStart = mpPlayer->GetCamera()->GetPosition();
          auto vDir = cMath::Vector3Normalize(handCenter - vStart);

          vEnd = vStart + vDir * mpPlayer->GetPickRay()->mfMaxDistance;

          mpPlayer->GetPickRay()->Clear();
          pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);
          mpPlayer->GetPickRay()->CalculateResults();

          if (mpPlayer->GetPickRay()->mpPickedBody != NULL &&
              mpPlayer->GetPickRay()->mpPickedBody == mpPlayer->GetPickRay()->mpPickedAreaBody)
            useTraceResults = true;
        }


        // Begin iteration, finding objects that could possibly be the user's target
        std::vector<touchedObject> targets;

        if (!useTraceResults) {
          cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
          iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

          ////////////////////////////////
          //Iterate bodies
          float fClosestHitDist = 9999.0f;
          cVector3f vClosestHitPos;
          iPhysicsMaterial* pClosestHitMat = NULL;
          cPhysicsBodyIterator it = pPhysicsWorld->GetBodyIterator();
          while (it.HasNext())
          {
            iPhysicsBody *pBody = it.Next();

            iGameEntity *pEntity = NULL;
            if (pBody->GetUserData())
              pEntity = (iGameEntity*)pBody->GetUserData();
            else
              continue;

            // Skip the object if it can't be interacted with anyway
            eCrossHairState interactState = pEntity->GetCompletePickCrosshairState(pBody);

            if (interactState == eCrossHairState_None)
              continue;

            cCollideData collideData;
            collideData.SetMaxSize(12);

            if (pPhysicsWorld->CheckShapeCollision(pBody->GetShape(), pBody->GetLocalMatrix(),
              collider,
              mtxTouch, collideData, 1) == false)
            {
              continue;
            }

            // These hueristic rules are stolen from the normal game's trace
            if (pEntity)
            {
              //Skip stick area
              if (pEntity->GetType() == eGameEntityType_StickArea) continue;

              //Area
              if ((pEntity->GetType() == eGameEntityType_Area ||
                pEntity->GetType() == eGameEntityType_StickArea ||
                pEntity->GetType() == eGameEntityType_DamageArea ||
                pEntity->GetType() == eGameEntityType_ForceArea))
              {
                // We no longer care about areas, since they're handled with the above raytrace to the hand's center.
                // Areas only seem to be important for things that don't require precision (i.e., you never interact with them)
                // Level change triggers, non-physics objects tend to be "area bodies"

                /*
                if (pEntity->GetDescription() != _W("") ||
                    pEntity->GetCallbackScript(eGameEntityScriptType_PlayerInteract) != NULL ||
                    pEntity->GetCallbackScript(eGameEntityScriptType_PlayerExamine) != NULL ||
                    pEntity->GetHasInteraction())
                {
                  vPickedAreaPos = VRHelper::CollideCenter(collideData.mvContactPoints, collideData.mlNumOfPoints);
                  fPickedAreaDist = eyeDist;
                  pPickedAreaBody = pBody;
                }
                */
              }
              else
              {
                float planeDistance = cMath::PlaneToPointDist(cPlanef(mpPlayer->GetCamera()->GetForward(), mpPlayer->GetCamera()->GetPosition()), pBody->GetBV()->GetWorldCenter());
                
                //Other entity
                touchedObject target = {
                  pBody,
                  planeDistance,
                  interactState,
                  collideData
                };

                targets.push_back(target);
              }
            }
          }

          float fPickedDist = 9998.0f;
          cVector3f vPickedPos;
          iPhysicsBody* pPickedBody = NULL;

          // Find the closest body
          touchedObject* closestTarget = NULL;
          float fClosestDist = 9999.0f;

          for (auto touchedBody : targets) {
            if (touchedBody.planeDistance > fClosestDist)
              continue;

            eCrossHairState interactType = touchedBody.crosshair;
            if (interactType == eCrossHairState_PickUp || interactType == eCrossHairState_Item) {
              // These types of objects require line-of-sight
              mpPlayer->GetPickRay()->Clear();
              pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), mpPlayer->GetCamera()->GetPosition(), touchedBody.physicsBody->GetWorldPosition(), true, false, true);
              mpPlayer->GetPickRay()->CalculateResults();

              if (mpPlayer->GetPickedBody() == touchedBody.physicsBody) {
                useTraceResults = true;

                break;
              }
            }
            else {
              closestTarget = &touchedBody;

              vPickedPos = VRHelper::CollideCenter(touchedBody.collideData.mvContactPoints, touchedBody.collideData.mlNumOfPoints);
              fPickedDist = touchedBody.planeDistance;
              pPickedBody = touchedBody.physicsBody;
            }
          }

          if (!useTraceResults) {
            mpPlayer->GetPickRay()->Clear();

            mpPlayer->GetPickRay()->mpPickedBody = pPickedBody;
            mpPlayer->GetPickRay()->mfPickedDist = fPickedDist;
            mpPlayer->GetPickRay()->mvPickedPos = vPickedPos;
          }
        }

        if (mpPlayer->GetPickedBody())
        {
          iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

          mpPlayer->SetHandCrossHairState(i, eCrossHairState_None);
          mpPlayer->SetHandCrossHairState(i, pEntity->GetCompletePickCrosshairState(mpPlayer->GetPickedBody()));

          break;
        }

        /*
        for (int j = 0; j < mpPlayer->GetPickOverlapCallback()->mvPickedEntityCenters.size(); j++) {
          vStart = poseMatrix.GetTranslation();
          vEnd = vStart + (vStart - mpPlayer->GetPickOverlapCallback()->mvPickedEntityCenters[i]) * 3.0f;

          mpPlayer->GetPickRay()->Clear();
          pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);
          mpPlayer->GetPickRay()->CalculateResults();

          if (mpPlayer->GetPickedBody())
          {
            iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

            auto handState = pEntity->GetPickCrossHairState(mpPlayer->GetPickedBody());

            if (handState != eCrossHairState_None && handState != eCrossHairState_Examine) {
              mpPlayer->SetHandCrossHairState(i, handState);
            }
            else {
              mpPlayer->SetCrossHairState(eCrossHairState_None);

              pEntity->PlayerPick();
              handState = mpPlayer->GetCrossHairState();

              mpPlayer->SetCrossHairState(lastState);
              mpPlayer->SetExamineBody(lastPick);

              if (handState != eCrossHairState_None && handState != eCrossHairState_Examine)
                mpPlayer->SetHandCrossHairState(i, handState);
            }

            break;
          }
        }
        */
      }
    }
  }

  /////////////////////////////////////////////////
  // Cast ray to see if anything could be examined.
  vStart = mpPlayer->GetCamera()->GetPosition();
  vEnd = vStart + mpPlayer->GetCamera()->GetForward() * mpPlayer->GetExamineRay()->mfMaxDistance;

  mpPlayer->GetExamineRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetExamineRay(), vStart, vEnd, true, false, true);
  mpPlayer->GetExamineRay()->CalculateResults();

  //Log("Picked body: %d\n",(size_t)mpPlayer->GetPickedBody());

  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    //Call entity
    pEntity->PlayerPick();

    //Set cross hair state
    eCrossHairState CrossState = pEntity->GetPickCrossHairState(mpPlayer->GetExamineBody(), true);

    if ((CrossState == eCrossHairState_Examine && !pEntity->GetHasBeenExamined()))
    {
      mpPlayer->SetCrossHairState(CrossState);
    }
    else
    {
      mpPlayer->SetCrossHairState(eCrossHairState_None);
    }
  }
  else
  {
    mpPlayer->SetCrossHairState(eCrossHairState_None);
  }
}

void cPlayerState_Normal_VR::OnPostSceneDraw() {
  /*
  cVector3f handSize(40.0f, 15.0f, 23.5f);

  auto lastState = mpPlayer->GetCrossHairState();
  auto lastPick = mpPlayer->GetExamineBody();

  auto collider = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld()->CreateBoxShape(handSize, NULL);

  {
    iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

    mpLowGfx->SetDepthTestActive(false);
    mpLowGfx->PushMatrix(eMatrix_ModelView);

    for (int i = 0; i < 2; ++i) {
      cMatrixf poseMatrix;

      if (mpInit->mpPlayerHands->GetCurrentModel(i) &&
          mpInit->mpPlayerHands->GetCurrentModel(i)->msName == "Hand") {

        auto hand = mpInit->mpPlayerHands->GetCurrentModel(i);

        hand->UpdatePoseMatrix(poseMatrix, 0.0f);

        cCamera3D *pCamera = static_cast<cCamera3D*>(mpInit->mpGame->GetScene()->GetCamera());
        cVector3f vCenter = poseMatrix.GetTranslation();

        auto mtxTouch = poseMatrix.GetRotation();
        mtxTouch = cMath::MatrixMul(cMath::MatrixTranslate(vCenter), mtxTouch);

        {
          cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
          iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

          ////////////////////////////////
          //Iterate bodies
          float fClosestHitDist = 9999.0f;
          cVector3f vClosestHitPos;
          iPhysicsMaterial* pClosestHitMat = NULL;
          cPhysicsBodyIterator it = pPhysicsWorld->GetBodyIterator();
          while (it.HasNext())
          {
            iPhysicsBody *pBody = it.Next();

            iGameEntity *pEntity = NULL;
            if (pBody->GetUserData())
              pEntity = (iGameEntity*)pBody->GetUserData();
            else
              continue;

            cVector3f center = pBody->GetBV()->GetWorldCenter();

            mpLowGfx->SetMatrix(eMatrix_ModelView, pCamera->GetViewMatrix());

            mpLowGfx->DrawSphere(center, 0.05f,
              cColor(1, 0, 0, 1));
          }
        }

        /*
          cCollideData collideData;
          collideData.SetMaxSize(12);

          if (mpInit->mpPlayerHands->GetCurrentModel(i) &&
            (mpInit->mpPlayerHands->GetCurrentModel(i)->msName == "Hand" ||
            mpInit->mpPlayerHands->GetCurrentModel(i)->msName == "LeftHand")) {

            auto hand = mpInit->mpPlayerHands->GetCurrentModel(i);

            hand->UpdatePoseMatrix(poseMatrix, 0.0f);

            cCamera3D *pCamera = static_cast<cCamera3D*>(mpInit->mpGame->GetScene()->GetCamera());
            cVector3f vCenter = poseMatrix.GetTranslation();

            auto mtxTouch = poseMatrix.GetRotation();
            mtxTouch = cMath::MatrixMul(cMath::MatrixTranslate(vCenter), mtxTouch);

            bool bCollide = false;

            float fPickedAreaDist;
            cVector3f vPickedAreaPos;
            iPhysicsBody* pPickedAreaBody = NULL;

            float fPickedDist;
            cVector3f vPickedPos;
            iPhysicsBody* pPickedBody = NULL;

            {
              cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
              iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

              ////////////////////////////////
              //Iterate bodies
              float fClosestHitDist = 9999.0f;
              cVector3f vClosestHitPos;
              iPhysicsMaterial* pClosestHitMat = NULL;
              cPhysicsBodyIterator it = pPhysicsWorld->GetBodyIterator();
              while (it.HasNext())
              {
                iPhysicsBody *pBody = it.Next();

                iGameEntity *pEntity = NULL;
                if (pBody->GetUserData())
                  pEntity = (iGameEntity*)pBody->GetUserData();
                else
                  continue;

                pBody->GetBV()->GetWorldCenter()

                collideData.mlNumOfPoints = 0;

                if (pPhysicsWorld->CheckShapeCollision(pBody->GetShape(), pBody->GetLocalMatrix(),
                  collider,
                  mtxTouch, collideData, 1) == false)
                {
                  continue;
                }

                if (pEntity)
                {
                  //Skip stick area
                  if (pEntity->GetType() == eGameEntityType_StickArea) continue;

                  //Area
                  if (pEntity->GetType() == eGameEntityType_Area ||
                    pEntity->GetType() == eGameEntityType_StickArea ||
                    pEntity->GetType() == eGameEntityType_DamageArea ||
                    pEntity->GetType() == eGameEntityType_ForceArea)
                  {
                    if (pEntity->GetDescription() != _W("") ||
                      pEntity->GetCallbackScript(eGameEntityScriptType_PlayerInteract) != NULL ||
                      pEntity->GetCallbackScript(eGameEntityScriptType_PlayerExamine) != NULL ||
                      pEntity->GetHasInteraction())
                    {
                      vPickedAreaPos = VRHelper::CollideCenter(collideData.mvContactPoints, collideData.mlNumOfPoints);
                      fPickedAreaDist = cMath::Vector3Dist(vCenter, vPickedAreaPos);
                      pPickedAreaBody = pBody;
                    }
                  }
                  //Other entity
                  else
                  {
                    vPickedPos = VRHelper::CollideCenter(collideData.mvContactPoints, collideData.mlNumOfPoints);
                    fPickedDist = cMath::Vector3Dist(vCenter, vPickedPos);
                    pPickedBody = pBody;
                  }
                }
              }
            }

            //Check if an area is closer than the closest normal body
            if (pPickedAreaBody && fPickedAreaDist < fPickedDist)
            {
              bool bUseArea = false;

              if (pPickedBody)
              {
                iGameEntity *pEntity = (iGameEntity*)pPickedBody->GetUserData();
                cGameArea *pArea = (cGameArea*)pPickedAreaBody->GetUserData();

                if (pEntity)
                {
                  //Too for from object
                  if (pEntity->GetMaxExamineDist() < fPickedDist) bUseArea = true;

                  //No description for object and it is not possible to interact with it
                  else if (pEntity->GetDescription() == _W(""))
                  {
                    if (pPickedBody->GetMass() == 0) bUseArea = true;

                    else if (pEntity->GetType() == eGameEntityType_Object)
                    {
                      cGameObject *pObject = static_cast<cGameObject*>(pEntity);

                      if (pObject->GetInteractMode() == eObjectInteractMode_Static) bUseArea = true;
                    }
                  }
                }
                else
                {
                  bUseArea = true;
                }
              }
              else
              {
                bUseArea = true;
              }

              if (bUseArea)
              {
                pPickedBody = pPickedAreaBody;
                fPickedDist = fPickedAreaDist;
                vPickedPos = vPickedAreaPos;
              }
            }

            cMatrixf mtxCollider = cMath::MatrixMul(pCamera->GetViewMatrix(), mtxTouch);

            mpLowGfx->SetMatrix(eMatrix_ModelView, mtxCollider);

            mpPlayer->GetPickRay()->Clear();

            mpPlayer->GetPickRay()->mpPickedAreaBody = pPickedAreaBody;
            mpPlayer->GetPickRay()->mfPickedAreaDist = fPickedAreaDist;
            mpPlayer->GetPickRay()->mvPickedAreaPos = vPickedAreaPos;

            mpPlayer->GetPickRay()->mpPickedBody = pPickedBody;
            mpPlayer->GetPickRay()->mfPickedDist = fPickedDist;
            mpPlayer->GetPickRay()->mvPickedPos = vPickedPos;

            if (mpPlayer->GetPickedBody())
            {
              iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

              auto handState = pEntity->GetPickCrossHairState(mpPlayer->GetPickedBody());

              if (handState != eCrossHairState_None && handState != eCrossHairState_Examine) {
                mpPlayer->SetHandCrossHairState(i, handState);
              }
              else {
                mpPlayer->SetCrossHairState(eCrossHairState_None);

                pEntity->PlayerPick();
                handState = mpPlayer->GetCrossHairState();

                mpPlayer->SetCrossHairState(lastState);
                mpPlayer->SetExamineBody(lastPick);

                if (handState != eCrossHairState_None && handState != eCrossHairState_Examine)
                  bCollide = true;
              }
            }
            

        cMatrixf mtxCollider = cMath::MatrixMul(pCamera->GetViewMatrix(), mtxTouch);

        mpLowGfx->SetMatrix(eMatrix_ModelView, mtxCollider);

        mpLowGfx->DrawBoxMaxMin(handSize*0.5, handSize*-0.5f,
          cColor(1, 0, 0, 1));
      }
    }

    mpLowGfx->PopMatrix(eMatrix_ModelView);
  }
    */

  iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

  cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
  //pWorld->GetPhysicsWorld()->RenderDebugGeometry(mpLowGfx, cColor(1.0f, 0.0f, 1.0f, 1.0f));

  {

    cCamera3D* pCamera3D = (cCamera3D*)mpInit->mpGame->GetScene()->GetCamera();

    for (int i = 0; i < 2; ++i) {
      auto player = mpInit->mpPlayer;
      auto crosshairState = player->GetHandCrossHairState(i);

      if (crosshairState == eCrossHairState_None)
        continue;

      auto crosshairMat = player->mvCrossHairs[crosshairState]->GetMaterial();

      mpLowGfx->SetDepthTestActive(false);
      mpLowGfx->PushMatrix(eMatrix_ModelView);

      cMatrixf iconMat = cMatrixf::Identity;

      // Rotate to face eyes
      iconMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), iconMat);

      // Move to hand's position
      cMatrixf handMat = VRHelper::ViveToWorldSpace(mpInit->mpGame->vr_right_hand.GetMatrix(), mpInit->mpGame);
      iconMat = cMath::MatrixMul(cMath::MatrixTranslate(handMat.GetTranslation()), iconMat);

      mpLowGfx->SetMatrix(eMatrix_ModelView, cMath::MatrixMul(pCamera3D->GetViewMatrix(), iconMat));

      auto ofs = crosshairMat->GetTextureOffset(eMaterialTexture_Diffuse);

      tVertexVec vtx;
      vtx.reserve(4);

      cColor color(1.0f, 1.0f);

      vtx[0] = cVertex(cVector3f(-0.035f, 0.035, 0), cVector2f(ofs.x, ofs.y), color);
      vtx[1] = cVertex(cVector3f(0.035, 0.035, 0), cVector2f(ofs.x + ofs.w, ofs.y), color);
      vtx[2] = cVertex(cVector3f(0.035, -0.035, 0), cVector2f(ofs.x + ofs.w, ofs.y + ofs.h), color);
      vtx[3] = cVertex(cVector3f(-0.035, -0.035, 0), cVector2f(ofs.x, ofs.y + ofs.h), color);

      mpLowGfx->SetTexture(0, crosshairMat->GetTexture(eMaterialTexture_Diffuse));
      mpLowGfx->SetBlendActive(true);
      mpLowGfx->SetBlendFunc(eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
      mpLowGfx->SetCullActive(false);

      mpLowGfx->DrawQuad(vtx);

      mpLowGfx->SetTexture(0, NULL);
      mpLowGfx->SetBlendActive(true);
      mpLowGfx->SetCullActive(true);

      mpLowGfx->PopMatrix(eMatrix_ModelView);
      mpLowGfx->SetDepthTestActive(true);
    }
  }

  // mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld()->DestroyShape(collider);
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartInteract()
{
  if (mpPlayer->GetPickedBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

    pEntity->PlayerInteract();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartExamine()
{
  //Log("Picked body: %d\n",(size_t)mpPlayer->GetPickedBody());
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartRun()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Walk ||
    mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
  }
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
  {
    mpPlayer->SetPrevMoveState(ePlayerMoveState_Run);
  }
}
void cPlayerState_Normal_VR::OnStopRun()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Run)
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  else if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
    mpPlayer->SetPrevMoveState(ePlayerMoveState_Walk);
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

  if (mpInit->mpButtonHandler->GetToggleCrouch())
  {
    if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
      mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
    else
      mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
  else
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
}

void cPlayerState_Normal_VR::OnStopCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
    mpInit->mpButtonHandler->GetToggleCrouch() == false)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartInteractMode()
{
  mpPlayer->ChangeState(ePlayerState_InteractMode);
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::EnterState(iPlayerState* apPrevState)
{
  mpPlayer->ResetCrossHairPos();
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::LeaveState(iPlayerState* apNextState)
{
  //Can cause crashes!!
  //mpPlayer->GetPickRay()->mpPickedBody = NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// INTERACT MODE STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_InteractMode_VR::cPlayerState_InteractMode_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_InteractMode)
{
  mvLookSpeed = 0;
  mfRange = 15.0f;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnUpdate(float afTimeStep)
{
  /////////////////////////////////////////////////
  // Move viewport.
  /*if(mvLookSpeed.x != 0)
  {
  mpPlayer->GetCamera()->AddYaw(mvLookSpeed.x * afTimeStep * 1.0f);
  mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }
  if(mvLookSpeed.y != 0)
  {
  mpPlayer->GetCamera()->AddPitch(mvLookSpeed.y * afTimeStep * 1.0f);
  }*/

  //LogUpdate("  Casting ray\n");
  /////////////////////////////////////////////////
  // Cast ray to see if anything is picked.
  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  cVector3f vDir = mpPlayer->GetCamera()->UnProject(
    mpPlayer->GetCrossHairPos(),
    mpInit->mpGame->GetGraphics()->GetLowLevel());
  vStart = mpPlayer->GetCamera()->GetPosition();
  vEnd = vStart + vDir * mpPlayer->GetPickRay()->mfMaxDistance;

  mpPlayer->GetPickRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);

  //LogUpdate("  Calc pickray results\n");	
  mpPlayer->GetPickRay()->CalculateResults();

  //LogUpdate("  Use picked body\n");	
  if (mpPlayer->GetPickedBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

    eCrossHairState CrossState = pEntity->GetPickCrossHairState(mpPlayer->GetPickedBody());

    if (CrossState == eCrossHairState_None)
      mpPlayer->SetCrossHairState(eCrossHairState_Inactive);
    else
      mpPlayer->SetCrossHairState(CrossState);

    pEntity->PlayerPick();
  }
  else
  {
    mpPlayer->SetCrossHairState(eCrossHairState_Inactive);
  }
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartInteract()
{
  if (mpPlayer->GetPickedBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

    pEntity->PlayerInteract();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_InteractMode_VR::OnAddYaw(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    mpPlayer->GetCamera()->AddYaw(-afVal * 2.0f * mpPlayer->GetLookSpeed());
    mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }
  else
  {
    if (mpPlayer->AddCrossHairPos(cVector2f(afVal * 800.0f, 0)))
    {
      mpPlayer->GetCamera()->AddYaw(-afVal * mpPlayer->GetLookSpeed());
      mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
    }

    cVector2f vBorder = mpPlayer->GetInteractMoveBorder();
    cVector2f vPos = mpPlayer->GetCrossHairPos();

    if (vPos.x < vBorder.x + mfRange || vPos.x >(799 - vBorder.x - mfRange))
    {
      float fDist;
      if (vPos.x < vBorder.x + mfRange)
      {
        fDist = (vPos.x - vBorder.x);
        mvLookSpeed.x = 1 - (fDist / mfRange) * 1;
      }
      else
      {
        fDist = ((799 - vBorder.x) - vPos.x);
        mvLookSpeed.x = (1 - (fDist / mfRange)) * -1;
      }
    }
    else
    {
      mvLookSpeed.x = 0;
    }
  }

  return false;
}

bool cPlayerState_InteractMode_VR::OnAddPitch(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    float fInvert = mpInit->mpButtonHandler->GetInvertMouseY() ? -1.0f : 1.0f;
    mpPlayer->GetCamera()->AddPitch(-afVal *2.0f*fInvert * mpPlayer->GetLookSpeed());
  }
  else
  {
    if (mpPlayer->AddCrossHairPos(cVector2f(0, afVal * 600.0f)))
    {
      mpPlayer->GetCamera()->AddPitch(-afVal * mpPlayer->GetLookSpeed());
    }

    cVector2f vBorder = mpPlayer->GetInteractMoveBorder();
    cVector2f vPos = mpPlayer->GetCrossHairPos();

    if (vPos.y < vBorder.y + mfRange || vPos.y >(599 - vBorder.y - mfRange))
    {
      float fDist;
      if (vPos.y < vBorder.y + mfRange)
      {
        fDist = (vPos.y - vBorder.y);
        mvLookSpeed.y = 1 - (fDist / mfRange) * 1;
      }
      else
      {
        fDist = ((599 - vBorder.y) - vPos.y);
        mvLookSpeed.y = (1 - (fDist / mfRange)) * -1;
      }
    }
    else
    {
      mvLookSpeed.y = 0;
    }
  }

  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartInteractMode()
{
  mpPlayer->ChangeState(ePlayerState_Normal);
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::EnterState(iPlayerState* apPrevState)
{
  mPrevMoveState = mpPlayer->GetMoveState();

  if (mpPlayer->GetMoveState() == ePlayerMoveState_Run)
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);

  mvLookSpeed = 0;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::LeaveState(iPlayerState* apNextState)
{
  /*if(mPrevMoveState != ePlayerMoveState_Run)
  mpPlayer->ChangeMoveState(mPrevMoveState);
  else
  mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);*/

  if (apNextState->mType == ePlayerState_Normal)
    mpPlayer->ResetCrossHairPos();

  //Can cause crashes!!
  //mpPlayer->GetPickRay()->mpPickedBody = NULL;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

  if (mpInit->mpButtonHandler->GetToggleCrouch())
  {
    if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
      mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
    else
      mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
  else
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
}

void cPlayerState_InteractMode_VR::OnStopCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
    mpInit->mpButtonHandler->GetToggleCrouch() == false)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  }
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// USE ITEM STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_UseItem_VR::cPlayerState_UseItem_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_UseItem)
{
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnUpdate(float afTimeStep)
{
  /////////////////////////////////////////////////
  // Cast ray to see if anything is picked.
  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  auto handMat = VRHelper::ViveToWorldSpace(mpInit->mpGame->vr_right_hand.GetMatrix(), mpInit->mpGame);

  vStart = handMat.GetTranslation();
  vEnd = vStart + cMath::MatrixInverse(handMat.GetRotation()).GetForward() * -1.0f;

  mvUseLineStart = vStart;
  mvUseLineEnd = vEnd;

  mpPlayer->GetPickRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);
  mpPlayer->GetPickRay()->CalculateResults();

  iPhysicsBody *pBody = mpPlayer->GetPickedBody();
  iGameEntity *pEntity = NULL;
  if (pBody) pEntity = (iGameEntity*)pBody->GetUserData();

  if (pEntity)
  {
    mvUseLineEnd = mpPlayer->GetPickedPos();
    mpInit->mpPlayer->SetItemFlash(true);
  }
  else
  {
    mpInit->mpPlayer->SetItemFlash(false);
  }
}

void cPlayerState_UseItem_VR::OnPostSceneDraw() {
  iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

  if (mpPlayer->GetItemFlash())
    mpLowGfx->DrawLine(mvUseLineStart, mvUseLineEnd, cColor(0, 1, 0, 0.75f));
  else
    mpLowGfx->DrawLine(mvUseLineStart, mvUseLineEnd, cColor(1, 0, 0, 0.75f));

  auto itemMat = mpPlayer->GetCurrentItem()->GetGfxObject()->GetMaterial();

  mpLowGfx->SetDepthTestActive(false);
  mpLowGfx->PushMatrix(eMatrix_ModelView);

  cMatrixf handMat = VRHelper::ViveToWorldSpace(mpInit->mpGame->vr_right_hand.GetMatrix(), mpInit->mpGame);
  mpLowGfx->SetMatrix(eMatrix_ModelView, cMath::MatrixMul(((cCamera3D*)mpInit->mpGame->GetScene()->GetCamera())->GetViewMatrix(), handMat ));

  auto ofs = itemMat->GetTextureOffset(eMaterialTexture_Diffuse);

  tVertexVec vtx;
  vtx.reserve(4);

  cColor color = mpPlayer->GetItemFlash() ? cColor(1.0f, 1.0f) : cColor(1.0f, 0.0f, 0.0f, 1.0f);

  float scaleFactor = 0.85f;

  vtx[0] = cVertex(cVector3f(-ofs.w * scaleFactor, ofs.h * scaleFactor, 0), cVector2f(ofs.x, ofs.y), color);
  vtx[1] = cVertex(cVector3f(ofs.w * scaleFactor, ofs.h * scaleFactor, 0), cVector2f(ofs.x + ofs.w, ofs.y), cColor(1.0f, 1.0f));
  vtx[2] = cVertex(cVector3f(ofs.w * scaleFactor, -ofs.h * scaleFactor, 0), cVector2f(ofs.x + ofs.w, ofs.y + ofs.h), cColor(1.0f, 1.0f));
  vtx[3] = cVertex(cVector3f(-ofs.w * scaleFactor, -ofs.h * scaleFactor, 0), cVector2f(ofs.x, ofs.y + ofs.h), cColor(1.0f, 1.0f));

  mpLowGfx->SetTexture(0, itemMat->GetTexture(eMaterialTexture_Diffuse));
  mpLowGfx->SetBlendActive(true);
  mpLowGfx->SetCullActive(false);

  mpLowGfx->DrawQuad(vtx);

  mpLowGfx->SetTexture(0, NULL);
  mpLowGfx->SetBlendActive(false);
  mpLowGfx->SetCullActive(true);

  mpLowGfx->PopMatrix(eMatrix_ModelView);
  mpLowGfx->SetDepthTestActive(true);
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnStartInteract()
{
  iPhysicsBody *pBody = mpPlayer->GetPickedBody();
  iGameEntity *pEntity = NULL;
  if (pBody) pEntity = (iGameEntity*)pBody->GetUserData();

  if (pEntity && mpPlayer->GetPickedDist() <= pEntity->GetMaxExamineDist())
  {
    if (mpPlayer->GetPickedDist() <= pEntity->GetMaxInteractDist())
    {
      iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();
      cGameItemType *pType = mpInit->mpInventory->GetItemType(mpPlayer->GetCurrentItem()->GetItemType());

      if (mPrevState == ePlayerState_WeaponMelee ||
        mPrevState == ePlayerState_Throw)
      {
        mpPlayer->ChangeState(ePlayerState_Normal);
      }
      else
      {
        mpPlayer->ChangeState(mPrevState);
      }

      pType->OnUse(mpPlayer->GetCurrentItem(), pEntity);
    }
    else
    {
      mpInit->mpEffectHandler->GetSubTitle()->Add(kTranslate("Player", "UseItemTooFar"), 2.0f, true);
      return;
    }
  }
  else
  {
    if (mPrevState == ePlayerState_WeaponMelee ||
      mPrevState == ePlayerState_Throw)
    {
      mpPlayer->ChangeState(ePlayerState_Normal);
    }
    else
    {
      mpPlayer->ChangeState(mPrevState);
    }
  }
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    if (mpPlayer->GetExamineDist() <= pEntity->GetMaxExamineDist())
    {
      pEntity->PlayerExamine();
    }
  }
  else
  {
    if (mPrevState == ePlayerState_WeaponMelee ||
      mPrevState == ePlayerState_Throw)
    {
      mpPlayer->ChangeState(ePlayerState_Normal);
    }
    else
    {
      mpPlayer->ChangeState(mPrevState);
    }

  }
}

//-----------------------------------------------------------------------

bool cPlayerState_UseItem_VR::OnAddYaw(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    mpPlayer->GetCamera()->AddYaw(-afVal * 2.0f * mpPlayer->GetLookSpeed());
    mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }
  else if (mpPlayer->AddCrossHairPos(cVector2f(afVal * 800.0f, 0)))
  {
    mpPlayer->GetCamera()->AddYaw(-afVal * mpPlayer->GetLookSpeed());
    mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }

  return false;
}

bool cPlayerState_UseItem_VR::OnAddPitch(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    float fInvert = mpInit->mpButtonHandler->GetInvertMouseY() ? -1.0f : 1.0f;
    mpPlayer->GetCamera()->AddPitch(-afVal *2.0f*fInvert * mpPlayer->GetLookSpeed());
  }
  else if (mpPlayer->AddCrossHairPos(cVector2f(0, afVal * 600.0f)))
  {
    mpPlayer->GetCamera()->AddPitch(-afVal * mpPlayer->GetLookSpeed());
  }

  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::EnterState(iPlayerState* apPrevState)
{
  mPrevMoveState = mpPlayer->GetMoveState();
  mPrevState = apPrevState->mType;

  mpPlayer->SetCrossHairState(eCrossHairState_Item);

  // Hide right hand
  mpPlayer->GetRightHand()->SetVisible(false);
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::LeaveState(iPlayerState* apNextState)
{
  mpPlayer->SetCrossHairState(eCrossHairState_None);

  // Show right hand
  mpPlayer->GetRightHand()->SetVisible(true);
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnStartCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

  if (mpInit->mpButtonHandler->GetToggleCrouch())
  {
    if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
      mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
    else
      mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
  else
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
}

void cPlayerState_UseItem_VR::OnStopCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
    mpInit->mpButtonHandler->GetToggleCrouch() == false)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_UseItem_VR::OnStartInventory()
{
  mpPlayer->ChangeState(mPrevState);
  return true;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// MESSAGE STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_Message_VR::cPlayerState_Message_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_Message)
{
}

//-----------------------------------------------------------------------


void cPlayerState_Message_VR::OnUpdate(float afTimeStep)
{
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnJump()
{
  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::OnStartInteract()
{
  mpInit->mpGameMessageHandler->ShowNext();
}

void cPlayerState_Message_VR::OnStopInteract()
{
}

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::OnStartExamine()
{
  mpInit->mpGameMessageHandler->ShowNext();
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnMoveForwards(float afMul, float afTimeStep) { return false; }
bool cPlayerState_Message_VR::OnMoveSideways(float afMul, float afTimeStep) { return false; }

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnAddYaw(float afVal) { return false; }
bool cPlayerState_Message_VR::OnAddPitch(float afVal) { return false; }

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::EnterState(iPlayerState* apPrevState)
{
  //Change move state so the player is still
  mPrevMoveState = mpPlayer->GetMoveState();
  //mpPlayer->ChangeMoveState(ePlayerMoveState_Still);

  mpPlayer->SetCrossHairState(eCrossHairState_None);
}

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::LeaveState(iPlayerState* apNextState)
{
  if (mPrevMoveState != ePlayerMoveState_Run && mPrevMoveState != ePlayerMoveState_Jump)
    mpPlayer->ChangeMoveState(mPrevMoveState);
  else
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnStartInventory()
{
  return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnStartInventoryShortCut(int alNum)
{
  return false;
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CLIMB STATE
//////////////////////////////////////////////////////////////////////////

static int NUM_LADDER_SOUNDS = 3;
static float LADDER_SOUND_SPACING = 0.65f;

cPlayerState_Climb_VR::cPlayerState_Climb_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_Climb)
{
  mpLadder = NULL;

  mfUpSpeed = mpInit->mpGameConfig->GetFloat("Movement_Climb", "UpSpeed", 0);
  mfDownSpeed = mpInit->mpGameConfig->GetFloat("Movement_Climb", "DownSpeed", 0);

  mfStepLength = mpInit->mpGameConfig->GetFloat("Movement_Climb", "StepLength", 0);

  mfStepCount = 0;

  mlState = 0;

  blackActive = false;
  blackAlpha = 0.0f;
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::PlaySound(const tString &asSound)
{
  if (asSound == "") return;

  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
  cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();

  cSoundEntity *pSound = pWorld->CreateSoundEntity("LadderStep", asSound, true);
  if (pSound) pSound->SetPosition(pCharBody->GetPosition());
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::EnterState(iPlayerState* apPrevState)
{
  mpPlayer->SetCrossHairState(eCrossHairState_None);

  mvPrevPitchLimits = mpPlayer->GetCamera()->GetPitchLimits();

  mlState = 0;
  mpPlayer->GetCharacterBody()->SetGravityActive(false);
  mpPlayer->GetCharacterBody()->SetTestCollision(false);

  mvGoalPos = mvStartPosition;
  mvGoalRot.x = 0;
  mvGoalRot.y = mpLadder->GetStartRotation().y;

  //Different time if you are above the ladder.
  float fTime = 0.5f;
  if (mpPlayer->GetCharacterBody()->GetPosition().y > mpLadder->GetMaxY()) fTime = 1.2f;

  cVector3f vStartRot;
  vStartRot.x = mpPlayer->GetCamera()->GetPitch();
  vStartRot.y = mpPlayer->GetCamera()->GetYaw();

  mvPosAdd = (mvGoalPos - mpPlayer->GetCharacterBody()->GetPosition()) / fTime;

  mvRotAdd.x = cMath::GetAngleDistance(vStartRot.x, mvGoalRot.x, k2Pif) / fTime;
  mvRotAdd.y = cMath::GetAngleDistance(vStartRot.y, mvGoalRot.y, k2Pif) / fTime;

  mvCharPosition = mpPlayer->GetCharacterBody()->GetPosition();

  mfTimeCount = fTime;

  mfStepCount = 0;

  mbPlayedSound = false;

  //Play Sound
  PlaySound(mpLadder->GetAttachSound());

  // For VR
  ladderSoundsLeft = max(2, (fabs(mpLadder->GetMaxY() - mpLadder->GetMinY()) / 1.2f));
  ladderSoundCount = LADDER_SOUND_SPACING;

  // Figure out if player is moving down or up ladder
  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
  cVector3f playerCenter = pCharBody->GetPosition() - pCharBody->GetSize().y * 0.5f;

  float topDist = cMath::Abs(playerCenter.y - mpLadder->GetMaxY());
  float bottomDist = cMath::Abs(playerCenter.y - mpLadder->GetMinY());

  if (topDist < bottomDist) {
    // Starting from top, moving to bottom of ladder
    ladderGoal = 0;

    mfLeaveAtTopCount = 0;
  }
  else {
    // Starting from bottom, moving to top of ladder
    ladderGoal = 1;

    mfLeaveAtTopCount = 2;
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::LeaveState(iPlayerState* apNextState)
{
  mpPlayer->SetCrossHairState(eCrossHairState_None);

  mpPlayer->GetCharacterBody()->SetGravityActive(true);
  mpPlayer->GetCharacterBody()->SetTestCollision(true);

  mpPlayer->GetCamera()->SetPitchLimits(mvPrevPitchLimits);
  mpPlayer->GetCamera()->SetYawLimits(cVector2f(0, 0));
}


//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnUpdate(float afTimeStep)
{
  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
  cCamera3D *pCam = mpPlayer->GetCamera();

  //////////////////////////////////
  // Attach To Ladder (for VR, fade screen to black)
  if (mlState == 0)
  {
    blackActive = true;

    if (blackAlpha < 1.0f) {
      blackAlpha += afTimeStep;
    }

    if (blackAlpha >= 1.0f) {
      blackAlpha = 1.0f;

      mlState++;
    }
  }
  //////////////////////////////////
  // Move On Ladder (for VR, play ladder sounds, wait for time)
  else if (mlState == 1)
  {
    if (ladderSoundsLeft > 0) {
      if (ladderSoundCount < 0) {
        if (ladderGoal == 0) // Moving down
          PlaySound(mpLadder->GetClimbDownSound());
        else // Moving up
          PlaySound(mpLadder->GetClimbUpSound());

        ladderSoundCount = LADDER_SOUND_SPACING;
        ladderSoundsLeft--;
      }
      else
        ladderSoundCount -= afTimeStep;
    }
    else {
      // No sounds to play left, wait some time then go to exit state
      ladderSoundCount -= afTimeStep;
      if (ladderSoundCount <= 0) {
        cVector3f exitPos = mvGoalPos;

        if (ladderGoal == 0) {
          // Set player to bottom of ladder
          exitPos.y = mpLadder->GetMinY() + 0.35f;
        }
        else {
          // Set player to top of ladder
          exitPos.y = mpLadder->GetMaxY();
          exitPos += pCharBody->GetForward();
        }

        pCharBody->SetFeetPosition(exitPos);

        mlState++;
      }
    }
  }
  //////////////////////////////////
  // On the top of the ladder
  else if (mlState == 2)
  {
    if (blackAlpha > 0.0f) {
      blackAlpha -= afTimeStep;
    }

    if (blackAlpha <= 0.0f) {
      blackAlpha = 0.0f;
      blackActive = false;

      mpPlayer->ChangeState(ePlayerState_Normal);
    }
  }

}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnStartInteract()
{
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnStartExamine()
{
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnAddYaw(float afVal)
{
  return false;
}

bool cPlayerState_Climb_VR::OnAddPitch(float afVal)
{
  return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnMoveForwards(float afMul, float afTimeStep)
{
  return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnMoveSideways(float afMul, float afTimeStep)
{
  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();



  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnStartCrouch()
{
}

void cPlayerState_Climb_VR::OnStopCrouch()
{

}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnJump()
{
  return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnStartInventory()
{
  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnPostSceneDraw() {
  if (!blackActive) return;

  iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

  auto pCamera3D = mpPlayer->GetCamera();
  cMatrixf uiViewMat = cMatrixf::Identity;

  // Translate to center of vision
  cMatrixf headInverse = cMath::MatrixInverse(pCamera3D->GetViewMatrix());
  cVector3f uiPos = headInverse.GetTranslation() + pCamera3D->GetViewMatrix().GetRotation().GetForward() * -0.75f;

  auto translateMat = cMath::MatrixTranslate(uiPos);

  auto centerTranslationMat = cMath::MatrixTranslate(cVector3f(-400.0f, -250.0f, 0.0f));
  uiViewMat = cMath::MatrixMul(centerTranslationMat, uiViewMat);

  // Rotate to face eyes
  uiViewMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), uiViewMat);

  // Translate in front of eyes
  uiViewMat = cMath::MatrixMul(translateMat, uiViewMat);

  // Project to world space
  uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), uiViewMat);

  mpLowGfx->SetMatrix(eMatrix_ModelView, uiViewMat);

  mpLowGfx->SetDepthTestActive(false);
  mpLowGfx->SetDepthWriteActive(false);

  mpLowGfx->SetTexture(0, NULL);
  mpLowGfx->SetBlendActive(true);
  mpLowGfx->SetBlendFunc(eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
  mpLowGfx->SetCullActive(false);

  mpLowGfx->DrawFilledRect2D(cRect2f(-9999.0f, -9999.0f, 99999.0f, 99999.0f), 0.0f, cColor(0.0f, 0.0f, 0.0f, blackAlpha));

  mpLowGfx->SetBlendActive(false);
  mpLowGfx->SetCullActive(true);

  mpLowGfx->SetDepthTestActive(true);
  mpLowGfx->SetDepthWriteActive(true);
}


//-----------------------------------------------------------------------