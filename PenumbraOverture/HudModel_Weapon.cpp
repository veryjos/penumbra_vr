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
#include "HudModel_Weapon.h"

#include "Init.h"
#include "Player.h"
#include "PlayerHelper.h"
#include "AttackHandler.h"
#include "GameEntity.h"
#include "GameEnemy.h"
#include "MapHandler.h"
#include "EffectHandler.h"

#include "GlobalInit.h"

#include "VRHelper.hpp"


//////////////////////////////////////////////////////////////////////////
// MELEE RAY CALLBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cMeleeRayCallback::Reset()
{
	mpClosestBody = NULL;
}

//-----------------------------------------------------------------------

bool cMeleeRayCallback::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
{
	if(pBody->GetCollide()==false) return true;
	if(pBody->IsCharacter()) return true;
	
	if(apParams->mfDist < mfShortestDist || mpClosestBody == NULL)
	{
		mpClosestBody = pBody;
		mfShortestDist = apParams->mfDist;
		mvPosition = apParams->mvPoint;
		mvNormal = apParams->mvNormal;
	}

	return true;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// HUD MODEL MELEE WEAPON
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cHudModel_WeaponMelee::cHudModel_WeaponMelee() : iHudModel(ePlayerHandType_WeaponMelee)
{
	ResetExtraData();

	if(gpInit->mbHasHaptics)
	{
		mpLowLevelHaptic = gpInit->mpGame->GetHaptic()->GetLowLevel();
		mpHHitForce = mpLowLevelHaptic->CreateSinusWaveForce(cVector3f(0,1,0),0.63f,5);
		mpHHitForce->SetActive(false);
	}
	else
	{
		mpLowLevelHaptic = NULL;
	}
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::LoadData(TiXmlElement *apRootElem)
{
	////////////////////////////////////////////////
	//Load the MAIN element.
	TiXmlElement *pMeleeElem = apRootElem->FirstChildElement("MELEE");
	if(pMeleeElem==NULL){
		Error("Couldn't load MELEE element from XML document\n");
		return;
	}

	mvHapticSize = cString::ToVector3f(pMeleeElem->Attribute("HapticSize"),0);
	mvHapticRot = cString::ToVector3f(pMeleeElem->Attribute("HapticRotate"),0);
	mfHapticScale = cString::ToFloat(pMeleeElem->Attribute("HapticScale"),2);

	mvHapticRot.x = cMath::ToRad(mvHapticRot.x);
	mvHapticRot.y = cMath::ToRad(mvHapticRot.y);
	mvHapticRot.z = cMath::ToRad(mvHapticRot.z);

  mvVRHBDamageSize = cString::ToVector3f(pMeleeElem->Attribute("VrHBDamageSize"), 0);
  mvVRHBTransOffset = cString::ToVector3f(pMeleeElem->Attribute("VrHBTransOffset"), 0);
	
	mbDrawDebug = cString::ToBool(pMeleeElem->Attribute("DrawDebug"),false);

	////////////////////////////////////////////////
	//Go through the ATTACK elements.
	TiXmlElement *pAttackElem = apRootElem->FirstChildElement("ATTACK");
	for(; pAttackElem != NULL; pAttackElem = pAttackElem->NextSiblingElement("ATTACK"))
	{
		cMeleeWeaponAttack meleeAttack;

        meleeAttack.mStart = GetPoseFromElem("StartPose",pAttackElem);
		meleeAttack.mEnd = GetPoseFromElem("EndPose",pAttackElem);
		meleeAttack.mfAttackLength = cString::ToFloat(pAttackElem->Attribute("AttackLength"),0);
		meleeAttack.mfChargeLength = cString::ToFloat(pAttackElem->Attribute("ChargeLength"),0);
		meleeAttack.mfTimeOfAttack = cString::ToFloat(pAttackElem->Attribute("TimeOfAttack"),0);

		meleeAttack.mfMaxImpulse = cString::ToFloat(pAttackElem->Attribute("MaxImpulse"),0);
		meleeAttack.mfMinImpulse = cString::ToFloat(pAttackElem->Attribute("MinImpulse"),0);
		meleeAttack.mfMinMass = cString::ToFloat(pAttackElem->Attribute("MinMass"),0);
		meleeAttack.mfMaxMass = cString::ToFloat(pAttackElem->Attribute("MaxMass"),0);
		meleeAttack.mfMinDamage = cString::ToFloat(pAttackElem->Attribute("MinDamage"),0);
		meleeAttack.mfMaxDamage = cString::ToFloat(pAttackElem->Attribute("MaxDamage"),0);

		meleeAttack.msSwingSound = cString::ToString(pAttackElem->Attribute("SwingSound"),"");
		meleeAttack.msChargeSound = cString::ToString(pAttackElem->Attribute("ChargeSound"),"");
		meleeAttack.msHitSound = cString::ToString(pAttackElem->Attribute("HitSound"),"");
		
		meleeAttack.mvSpinMul = cString::ToVector3f(pAttackElem->Attribute("SpinMul"),0);

		meleeAttack.mfDamageRange = cString::ToFloat(pAttackElem->Attribute("DamageRange"),0);
		meleeAttack.mvDamageSize = cString::ToVector3f(pAttackElem->Attribute("DamageSize"),0);

		meleeAttack.mfAttackRange = cString::ToFloat(pAttackElem->Attribute("AttackRange"),0);

		meleeAttack.mfAttackSpeed = cString::ToFloat(pAttackElem->Attribute("AttackSpeed"),0);
		meleeAttack.mlAttackStrength = cString::ToInt(pAttackElem->Attribute("AttackStrength"),0);

		meleeAttack.msHitPS = cString::ToString(pAttackElem->Attribute("HitPS"),"");
		meleeAttack.mlHitPSPrio = cString::ToInt(pAttackElem->Attribute("HitPSPrio"),0);
	
		//Get largest side and use that to make bounding box.
		float fMax = meleeAttack.mvDamageSize.x;
		if(fMax < meleeAttack.mvDamageSize.y) fMax = meleeAttack.mvDamageSize.y;
		if(fMax < meleeAttack.mvDamageSize.z) fMax = meleeAttack.mvDamageSize.z;

		meleeAttack.mBV.SetSize(fMax * kSqrt2f);

		mvAttacks.push_back(meleeAttack);
	}
}

//-----------------------------------------------------------------------

bool cHudModel_WeaponMelee::UpdatePoseMatrix(cMatrixf& aPoseMtx, float afTimeStep)
{
  mmLastPose = GetEntity()->GetWorldMatrix();

  bool result = iHudModel::UpdatePoseMatrix(aPoseMtx, afTimeStep);

  if (result)
    Attack(aPoseMtx);

  return result;
}

/*
bool cHudModel_WeaponMelee::UpdatePoseMatrix(cMatrixf& aPoseMtx, float afTimeStep)
{
	////////////////////////
	//Idle and waiting for movement
  auto vr_scaleMtx = cMath::MatrixScale(cVector3f(mfVrScale));
  auto vr_rotMtx = cMath::MatrixRotate(mvVrRotOffset, eEulerRotationOrder_XYZ);

  aPoseMtx = mpInit->mpGame->vr_right_hand.GetMatrix();
 
  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_rotMtx);
  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_scaleMtx);

  return true;

  ///*
	if(mlAttackState<=1)
	{
		return false;
	}
	////////////////////
	//Movement
	else
	{
		aPoseMtx = cMath::MatrixSlerp(mfTime,m_mtxPrevPose,m_mtxNextPose,true);
		
		float fMul = 1.0f;
		//if(mlAttackState == 2 && mpInit->mDifficulty== eGameDifficulty_Easy) fMul = 1.6f;

		mfTime += mfMoveSpeed * afTimeStep * fMul;
		
		//Attack
		if(mlAttackState == 4 &&mfTime >= mvAttacks[mlCurrentAttack].mfTimeOfAttack && mbAttacked==false)
		{
			Attack();
			mbAttacked = true;
		}

		//Time is up
		if(mfTime >= 1.0f)
		{
			mfTime =1.0f;
			

			switch(mlAttackState)
			{
			case 2:
				mlAttackState = 3;
				break;
			case 4:
				mlAttackState = 5;
				mbAttacked = false;

				m_mtxPrevPose = m_mtxNextPose;
				m_mtxNextPose = mEquipPose.ToMatrix();

				mfMoveSpeed = 2;
				mfTime =0;

				break;
			case 5:
				if(mbButtonDown)	mlAttackState = 1;
				else				mlAttackState = 0;
								
				break;
			}

		}

		return true;
	}

	return false;
}
  */

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::OnAttackDown()
{
	if(mState == eHudModelState_Idle && mlAttackState ==0)
	{
		mlAttackState = 1;
		mfTime =0;

		mbButtonDown = true;
	}
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::OnAttackUp()
{
	if(mpInit->mbSimpleWeaponSwing)
	{

	}
	else
	{
		if(mlAttackState != 0 && mlAttackState != 4 && mlAttackState != 5)
		{
			mlAttackState = 5;

			mfMoveSpeed = 2;
			mfTime =0;

			m_mtxPrevPose = m_mtxNextPose;
			m_mtxNextPose = mEquipPose.ToMatrix();
		}
	}

	mbButtonDown = false;
}

//-----------------------------------------------------------------------

bool cHudModel_WeaponMelee::OnMouseMove(const cVector2f &avMovement)
{
	float fMinMovement = 0.015f;

	if(mlAttackState ==0 || (mbButtonDown==false && mpInit->mbSimpleWeaponSwing==false))
	{
		return true;
	}
	else
	{
		/////////////////////////////
		//Check for charge
		if(mlAttackState == 1)
		{
			if(mpInit->mbSimpleWeaponSwing)
			{
				//if(avMovement.y < -0.03f)
				//	mlCurrentAttack = 2;
				//else
					mlCurrentAttack = 0;//cMath::RandRectl(0,1);
				
				mlAttackState =2;
			}
			//Right charge
			else if(avMovement.x > fMinMovement)
			{
				mlCurrentAttack = 0;
				mlAttackState = 2;
			}
			//Left charge
			else if(avMovement.x < -fMinMovement)
			{
				mlCurrentAttack = 1;
				mlAttackState = 2;
			}
			//Down charge
			else if(avMovement.y > fMinMovement)
			{
				mlCurrentAttack = 2;
				mlAttackState = 2;
			}
			
			//Go to charge
			if(mlAttackState==2)
			{
				mfTime = 0.0f;
				mfMoveSpeed = 1/mvAttacks[mlCurrentAttack].mfChargeLength;

				//if(mpInit->mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
				//	mfMoveSpeed *= 0.8f;

				PlaySound(mvAttacks[mlCurrentAttack].msChargeSound);
				
				m_mtxPrevPose = mEquipPose.ToMatrix();
				m_mtxNextPose = mvAttacks[mlCurrentAttack].mStart.ToMatrix();
			}
			
		}
		else if(mlAttackState == 3)
		{
			//If right key is down enable looking.
			cInput *pInput = mpInit->mpGame->GetInput();
			if(pInput->IsTriggerd("Examine")) return true;
			
			if(mpInit->mbSimpleWeaponSwing)
			{
				if(mlCurrentAttack != 2 && pInput->IsTriggerd("Interact")==false)
				{
					mfTime = 0.0f;
					mfMoveSpeed = 1/mvAttacks[mlCurrentAttack].mfChargeLength;
					//if(mpInit->mpPlayer->GetMoveState() == ePlayerMoveState_Crouch) 	mfMoveSpeed *= 0.8f;
					
					m_mtxPrevPose = mvAttacks[mlCurrentAttack].mStart.ToMatrix();
					m_mtxNextPose = mvAttacks[2].mStart.ToMatrix();

					mlCurrentAttack = 2;
					mlAttackState = 2;
				}
				else
				{
					mlAttackState = 4;
				}
			}
			else if(mlCurrentAttack==0)
			{
				if(avMovement.x < -fMinMovement)
				{
					mlAttackState = 4;
				}
			}
			else if(mlCurrentAttack==1)
			{
				if(avMovement.x > fMinMovement)
				{
					mlAttackState = 4;
				}
			}
			else if(mlCurrentAttack==2)
			{
				if(avMovement.y < -fMinMovement)
				{
					mlAttackState = 4;
				}
			}

			if(mlAttackState == 4)
			{
				mfTime = 0.0f;
				mfMoveSpeed = 1.0f/mvAttacks[mlCurrentAttack].mfAttackLength;

				//if(mpInit->mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
				//	mfMoveSpeed *= 0.55f;
				
				PlaySound(mvAttacks[mlCurrentAttack].msSwingSound);

				mpInit->mpPlayer->GetHidden()->UnHide();

				m_mtxPrevPose = m_mtxNextPose;
				m_mtxNextPose = mvAttacks[mlCurrentAttack].mEnd.ToMatrix();
			}
		}
		
		return mpInit->mbSimpleWeaponSwing;
	}
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::PlaySound(const tString &asSound)
{
	cSoundHandler *pSoundHandler = mpInit->mpGame->GetSound()->GetSoundHandler();

	pSoundHandler->PlayGui(asSound,false,1.0f);
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::LoadExtraEntites()
{
	iPhysicsWorld *pWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
	
	for(size_t i=0; i< mvAttacks.size(); ++i)
	{
		//Attack shapes
		mvAttacks[i].mpCollider = pWorld->CreateBoxShape(mvAttacks[i].mvDamageSize,NULL);

		//Preload particle system
		mpInit->PreloadParticleSystem(mvAttacks[i].msHitPS);
		
		//Preload sounds
		mpInit->PreloadSoundEntityData(mvAttacks[i].msHitSound);
		mpInit->PreloadSoundEntityData(mvAttacks[i].msSwingSound);
		mpInit->PreloadSoundEntityData(mvAttacks[i].msChargeSound);
	}

  mpVrCollider = pWorld->CreateBoxShape(mvVRHBDamageSize, NULL);
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::DestroyExtraEntities()
{
	iPhysicsWorld *pWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();

	for(size_t i=0; i< mvAttacks.size(); ++i)
	{
		if(mvAttacks[i].mpCollider)
			pWorld->DestroyShape(mvAttacks[i].mpCollider);
	}

  pWorld->DestroyShape(mpVrCollider);
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::PostSceneDraw()
{
  /*
  cVector3f weaponPos = GetEntity()->GetLocalPosition();

	cCamera3D *pCamera = static_cast<cCamera3D*>(mpInit->mpGame->GetScene()->GetCamera());
	float fAttackRange = mvAttacks[mlCurrentAttack].mfAttackRange;
	
	float fDamageRange = mvAttacks[mlCurrentAttack].mfDamageRange;	
	cVector3f vCenter = weaponPos;

  cMatrixf mtxDamage = cMath::MatrixTranslate(mvVRHBTransOffset);
  mtxDamage = cMath::MatrixMul(cMath::MatrixMul(cMath::MatrixInverse(cMath::MatrixScale(mpInit->mpPlayerHands->GetCurrentModel(1)->mfVrScale)), GetEntity()->GetLocalMatrix().GetRotation()), mtxDamage);
	mtxDamage = cMath::MatrixMul(cMath::MatrixTranslate(vCenter), mtxDamage);

	bool bCollide=false;
	{
		cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
		iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();
		
		bCollide = pPhysicsWorld->CheckShapeWorldCollision(NULL,mpVrCollider,
															mtxDamage,NULL,false,false,NULL,false);
	} 

	cMatrixf mtxCollider = cMath::MatrixMul(pCamera->GetViewMatrix(),mtxDamage);
		
	mpInit->mpGame->GetGraphics()->GetLowLevel()->SetMatrix(eMatrix_ModelView,mtxCollider);

	
	if(bCollide)
		mpInit->mpGame->GetGraphics()->GetLowLevel()->DrawBoxMaxMin(mvVRHBDamageSize*0.5f, mvVRHBDamageSize*-0.5f,
																cColor(0,1,0,1));
	else
		mpInit->mpGame->GetGraphics()->GetLowLevel()->DrawBoxMaxMin(mvVRHBDamageSize*0.5f, mvVRHBDamageSize*-0.5f,
																cColor(1,0,1,1));
  */
}

//-----------------------------------------------------------------------

bool cHudModel_WeaponMelee::IsAttacking()
{
	if(mlAttackState >1) return true;

	return false;
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::ResetExtraData()
{
	mlAttackState = 0;
	mfTime =0;

  mpLastBodies.clear();

	mlCurrentAttack =0;

	mbButtonDown = false;
	mbAttacked = false;

	m_mtxPrevPose = cMatrixf::Identity;
	m_mtxNextPose = cMatrixf::Identity;

	mfMoveSpeed = 1.0f;

  mbHasHit = false;
  mfHitCooldown = 0.0f;
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::Attack(cMatrixf newPose)
{
	mpInit->mbWeaponAttacking = true;
	//Log("----------------- BEGIN ATTACK WITH WEAPON ------------ \n");

	////////////////////////////////
	//Set up
	float fDamageRange = mvAttacks[mlCurrentAttack].mfDamageRange;	

	float fMaxImpulse = mvAttacks[mlCurrentAttack].mfMaxImpulse;
	float fMinImpulse = mvAttacks[mlCurrentAttack].mfMinImpulse;

	float fMaxMass = mvAttacks[mlCurrentAttack].mfMaxMass;
	float fMinMass = mvAttacks[mlCurrentAttack].mfMinMass;

  cVector3f weaponPos = newPose.GetTranslation();
	
	cCamera3D *pCamera = mpInit->mpPlayer->GetCamera(); 
	cVector3f vCenter = weaponPos;
  cVector3f lastvCenter = mmLastPose.GetTranslation();

	cVector3f vSpinMul = cVector3f(0, 1.0f, 0.0f);
	vSpinMul =	pCamera->GetRight() * vSpinMul.x +
				pCamera->GetUp() * vSpinMul.y +
				pCamera->GetForward() * vSpinMul.z;

  cMatrixf mtxDamage = cMath::MatrixTranslate(mvVRHBTransOffset);
  mtxDamage = cMath::MatrixMul(cMath::MatrixInverse(cMath::MatrixScale(mpInit->mpPlayerHands->GetCurrentModel(1)->mfVrScale)), mtxDamage);
  mtxDamage = cMath::MatrixMul(newPose.GetRotation(), mtxDamage);
  mtxDamage = cMath::MatrixMul(cMath::MatrixTranslate(vCenter), mtxDamage);

  cVector3f hitboxCenterPos = cVector3f(0.0f, 0.0f, 0.0f);
  hitboxCenterPos = cMath::MatrixMul(mtxDamage, hitboxCenterPos);

  cMatrixf mtxOldDamage = cMath::MatrixTranslate(mvVRHBTransOffset);
  mtxOldDamage = cMath::MatrixMul(cMath::MatrixInverse(cMath::MatrixScale(mpInit->mpPlayerHands->GetCurrentModel(1)->mfVrScale)), mtxOldDamage);
  mtxOldDamage = cMath::MatrixMul(mmLastPose.GetRotation(), mtxOldDamage);
  mtxOldDamage = cMath::MatrixMul(cMath::MatrixTranslate(lastvCenter), mtxOldDamage);

  cVector3f lastHitboxCenterPos = cVector3f(0.0f, 0.0f, 0.0f);
  lastHitboxCenterPos = cMath::MatrixMul(mtxOldDamage, lastHitboxCenterPos);

  cBoundingVolume tempBV = mpVrCollider->GetBoundingVolume();
  tempBV.SetTransform(mtxDamage);

	cCollideData collideData;
	collideData.SetMaxSize(1);
	    
	bool bHit = false;

	cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	tVector3fList lstPostions;

	////////////////////////////////
	//Iterate Enemies
	tGameEnemyIterator enemyIt = mpInit->mpMapHandler->GetGameEnemyIterator();
	while(enemyIt.HasNext())
	{
		iGameEnemy *pEnemy = enemyIt.Next();
		iPhysicsBody *pBody = pEnemy->GetMover()->GetCharBody()->GetBody();
		float fMass = pBody->GetMass();

		if(pEnemy->GetMover()->GetCharBody()->IsActive()==false) continue;

		if(cMath::CheckCollisionBV(tempBV, *pBody->GetBV()))
		{
			/*if(pPhysicsWorld->CheckShapeCollision(pBody->GetShape(),pBody->GetLocalMatrix(),
				mvAttacks[mlCurrentAttack].mpCollider,
				mtxDamage,collideData,1)==false)
			{
				continue;
			}*/
			if(pEnemy->GetMeshEntity()->CheckColliderShapeCollision(pPhysicsWorld,
																	mpVrCollider,
																	mtxDamage,&lstPostions,NULL)==false)
			{
				continue;
			}

			//Calculate force
      float fForceSize = (hitboxCenterPos - lastHitboxCenterPos).Length();

			cVector3f vForceDir = hitboxCenterPos - lastHitboxCenterPos;
			vForceDir.Normalise();
			
			//Add force to bodies
			for(int i=0; i < pEnemy->GetBodyNum(); ++i)
			{
				iPhysicsBody* pBody = pEnemy->GetBody(i);
				
				pBody->AddImpulse(vForceDir *fForceSize*0.5f);

				cVector3f vTorque = vSpinMul * fMass * fForceSize *0.5f;
				pBody->AddTorque(vTorque);
			}

			//Calculate damage
			float fDamage = cMath::RandRectf(	mvAttacks[mlCurrentAttack].mfMinDamage,
												mvAttacks[mlCurrentAttack].mfMaxDamage);

      if (!mbHasHit && mfHitCooldown <= 0.0f) {
        pEnemy->Damage(fDamage, mvAttacks[mlCurrentAttack].mlAttackStrength);

        //Get closest position
        float fClosestDist = 9999.0f;
        cVector3f vClosestPostion = vCenter;
        for (tVector3fListIt it = lstPostions.begin(); it != lstPostions.end(); ++it)
        {
          cVector3f &vPos = *it;

          float fDist = cMath::Vector3DistSqr(hitboxCenterPos, vPos);
          if (fDist < fClosestDist)
          {
            fClosestDist = fDist;
            vClosestPostion = vPos;
          }
        }

        //Particle system
        if (pEnemy->GetHitPS() != "")
        {
          pWorld->CreateParticleSystem("Hit", pEnemy->GetHitPS(), 1,
            cMath::MatrixTranslate(vClosestPostion));
        }
      }

			lstPostions.clear();
		
			bHit = true;
		}
	}
	
	std::set<iPhysicsBody*> m_setHitBodies;

	////////////////////////////////
	//Iterate bodies
	float fClosestHitDist = 9999.0f;
	cVector3f vClosestHitPos;
	iPhysicsMaterial* pClosestHitMat = NULL;
	cPhysicsBodyIterator it = pPhysicsWorld->GetBodyIterator();
	while(it.HasNext())
	{
		iPhysicsBody *pBody = it.Next();

		float fMass = pBody->GetMass();

		if(pBody->IsActive()==false) continue;
		if(pBody->GetCollide()==false) continue;
		if(pBody->IsCharacter()) continue;

		
		if(cMath::CheckCollisionBV(tempBV, *pBody->GetBV()))
		{
			if(pPhysicsWorld->CheckShapeCollision(pBody->GetShape(),pBody->GetLocalMatrix(),
											mpVrCollider,
											mtxDamage,collideData,1)==false)
			{
				continue;
			}

			m_setHitBodies.insert(pBody);
			
			//Deal damage and force
      if (std::find(mpLastBodies.begin(), mpLastBodies.end(), pBody) == mpLastBodies.end()) {
        {
          iGameEntity *pEntity = (iGameEntity*)pBody->GetUserData();

          if (pEntity && pEntity->GetType() == eGameEntityType_Enemy) return;

          cCamera3D *pCamera = mpInit->mpPlayer->GetCamera();

          float fMass = pBody->GetMass();

          float fMaxImpulse = mvAttacks[mlCurrentAttack].mfMaxImpulse;
          float fMinImpulse = mvAttacks[mlCurrentAttack].mfMinImpulse;

          float fMaxMass = mvAttacks[mlCurrentAttack].mfMaxMass;
          float fMinMass = mvAttacks[mlCurrentAttack].mfMinMass;

          //Calculate force
          float fForceSize = (hitboxCenterPos - lastHitboxCenterPos).Length() * 18000.0f;

          if (fForceSize > 320.0f) {
            cVector3f vForceDir = hitboxCenterPos - lastHitboxCenterPos;
            vForceDir.Normalise();

            //Calculate damage
            float fDamage = cMath::RandRectf(mvAttacks[mlCurrentAttack].mfMinDamage,
              mvAttacks[mlCurrentAttack].mfMaxDamage);

            if (fMass > 0 && fForceSize > 0)
            {
              pBody->AddForceAtPosition(vForceDir * fForceSize, hitboxCenterPos);
            }

            if (pEntity)
            {
              pEntity->SetLastImpulse(vForceDir * fForceSize);
              pEntity->Damage(fDamage, mvAttacks[mlCurrentAttack].mlAttackStrength);
            }

            vClosestHitPos = VRHelper::CollideCenter(collideData.mvContactPoints, collideData.mlNumOfPoints);
            pClosestHitMat = pBody->GetMaterial();
          }

          mpLastBodies.push_back(pBody);
        }
      }
			
			bHit = true;
		}
	}
  /*
	////////////////////////////////////////////
	//Check with ray and see a closer material can be found.
	{
		float fAttackRange = mvAttacks[mlCurrentAttack].mfAttackRange;

		mRayCallback.Reset();
    cVector3f checkDir = hitboxCenterPos - lastHitboxCenterPos;
		cVector3f vRayStart = hitboxCenterPos;
    cVector3f vRayEnd = hitboxCenterPos + checkDir * 0.25f;

		pPhysicsWorld->CastRay(&mRayCallback,vRayStart,vRayEnd,true,true,true,false);

		if(mRayCallback.mpClosestBody)
		{
			//Use ray cast to check hit as well
			//Check first if body has not allready been hit.
			if(m_setHitBodies.find(mRayCallback.mpClosestBody)==m_setHitBodies.end())
			{
				HitBody(mRayCallback.mpClosestBody);
			}

			float fDist = cMath::Vector3DistSqr(mRayCallback.mvPosition, pCamera->GetPosition());
			if(fDist < fClosestHitDist)
			{
				fClosestHitDist = fDist;
				vClosestHitPos = mRayCallback.mvPosition;
				pClosestHitMat = mRayCallback.mpClosestBody->GetMaterial();
			}
		}
	}
  */
		
	////////////////////////////////////////////
	//Check the closest material and play sounds and effects depending on it.
	if(pClosestHitMat && !mbHasHit && mfHitCooldown <= 0.0f)
	{
		bHit = true;

		cMatrixf mtxPosition = cMath::MatrixTranslate(vClosestHitPos);

		cSurfaceData *pData = pClosestHitMat->GetSurfaceData();
		cSurfaceImpactData *pImpact = pData->GetHitDataFromSpeed(mvAttacks[mlCurrentAttack].mfAttackSpeed);
		if(pImpact)
		{
			cSoundEntity *pSound = pWorld->CreateSoundEntity("Hit",pImpact->GetSoundName(),true);
			if(pSound) pSound->SetPosition(vClosestHitPos);

			if(mvAttacks[mlCurrentAttack].mlHitPSPrio <= pImpact->GetPSPrio())
			{
				if(pImpact->GetPSName()!="")
					pWorld->CreateParticleSystem("Hit",pImpact->GetPSName(),1,mtxPosition);
			}
			else
			{
				if(mvAttacks[mlCurrentAttack].msHitPS!="")
					pWorld->CreateParticleSystem("Hit",mvAttacks[mlCurrentAttack].msHitPS,1,mtxPosition);
			}
		}

		PlaySound(mvAttacks[mlCurrentAttack].msHitSound);

		if(mpInit->mbHasHaptics)
		{
			if(mpHHitForce->IsActive())	mpHHitForce->SetActive(false);
			
			mpHHitForce->SetActive(true);
			mpHHitForce->SetTimeControl(false,0.3f, 0.2f, 0,0.1f);
		}

    mbHasHit = true;
	}

  if (!bHit) {
    if (mfHitCooldown > 0.0f)
      mfHitCooldown -= 1.0f; // * frametime
    else {
      mbHasHit = false;
      mpLastBodies.clear();
    }
  }
  else {
    mfHitCooldown = 15.0f;
  }

	mpInit->mbWeaponAttacking = false;
}

//-----------------------------------------------------------------------

void cHudModel_WeaponMelee::HitBody(iPhysicsBody *apBody)
{
}

//-----------------------------------------------------------------------
