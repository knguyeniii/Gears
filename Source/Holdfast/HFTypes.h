#pragma once

#include "CoreMinimal.h"

/** Static tuning for one weapon. */
struct FHFWeaponStats
{
	FString Name = TEXT("Weapon");
	float Damage = 10.f;
	float FireInterval = 0.1f;
	int32 MagSize = 30;
	float ReloadTime = 1.8f;
	float SpreadDeg = 1.5f;
	int32 Pellets = 1;
	float Range = 8000.f;
	float FalloffStart = 2500.f;
	bool bAuto = true;
};

/** Runtime state for one carried weapon. */
struct FHFWeapon
{
	FHFWeaponStats Stats;
	int32 Ammo = 0;
	int32 Reserve = 0;
	float NextFireTime = 0.f;
	bool bReloading = false;
	float ReloadEndTime = 0.f;
};

/** A position an AI can crouch at, on one side of a low cover piece. */
struct FHFCoverSlot
{
	FVector Location = FVector::ZeroVector;
	/** Points away from the cover piece, toward the slot. */
	FVector Normal = FVector::ForwardVector;
	TWeakObjectPtr<AActor> Claimant;
};

enum class EHFEnemyKind : uint8
{
	Drone,   // rifle, fights from cover
	Skitter, // fast melee rusher
	Hauler   // slow, armoured, suppressing fire
};
