#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HFTypes.h"
#include "HFCharacterBase.generated.h"

class UPointLightComponent;
class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

/** Shared health, weapons, hitscan fire and the primitive-built body. */
UCLASS(Abstract)
class HOLDFAST_API AHFCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AHFCharacterBase();

	virtual void Tick(float DeltaSeconds) override;

	virtual void ReceiveHit(float Damage, AHFCharacterBase* From, const FVector& FromLocation);

	bool IsDead() const { return bDead; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	int32 GetTeamId() const { return TeamId; }
	float GetLastDamageTime() const { return LastDamageTime; }
	const FHFWeapon* GetCurrentWeapon() const { return Weapons.IsValidIndex(WeaponIdx) ? &Weapons[WeaponIdx] : nullptr; }

protected:
	UPROPERTY()
	TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY()
	TObjectPtr<UPointLightComponent> MuzzleLight;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	float MaxHealth = 100.f;
	float Health = 100.f;
	int32 TeamId = 0;
	bool bDead = false;
	float DeathTime = 0.f;
	float LastDamageTime = -100.f;
	float LowPoseAlpha = 0.f;
	bool bForceLowPose = false;
	float MuzzleOffTime = 0.f;
	FLinearColor TracerColor = FLinearColor(1.f, 0.8f, 0.4f);

	TArray<FHFWeapon> Weapons;
	int32 WeaponIdx = 0;

	void BuildBody(const FLinearColor& Armor, const FLinearColor& Under, const FLinearColor& Accent);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& RelLoc, const FVector& Size, const FLinearColor& Color);

	void AddWeapon(const FHFWeaponStats& Stats, int32 Reserve);
	/** Fires the current weapon if it is ready. Returns true if a shot left the barrel. */
	bool FireShot(const FVector& Muzzle, const FVector& AimPoint, float SpreadMul);
	void StartReload();

	virtual void Die(AHFCharacterBase* Killer);
	virtual void OnDealtDamage(bool bKilled, bool bHeadshot) {}
};
