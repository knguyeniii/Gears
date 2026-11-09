#pragma once

#include "CoreMinimal.h"
#include "HFCharacterBase.h"
#include "HFPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UPrimitiveComponent;

/** Over-the-shoulder player: snap-to-cover, pop-up / corner peek, blind fire, vault, dodge roll. */
UCLASS()
class HOLDFAST_API AHFPlayerCharacter : public AHFCharacterBase
{
	GENERATED_BODY()

public:
	AHFPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsInCover() const { return bInCover; }
	bool IsAiming() const { return bAiming; }
	bool IsSprinting() const { return bSprinting; }
	float GetSpreadMul() const;
	float GetLastHitMarkerTime() const { return LastHitMarkerTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	void RefillAmmo();

	/** Flip if your mouse feels inverted. */
	bool bInvertLookY = false;

protected:
	virtual void OnDealtDamage(bool bKilled, bool bHeadshot) override;

private:
	UPROPERTY()
	TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	// Cover state
	bool bInCover = false;
	bool bCoverLow = false;
	FVector CoverNormal = FVector::ForwardVector;
	TWeakObjectPtr<UPrimitiveComponent> CoverComp;
	float CoverFacingSign = 1.f;
	int32 PeekSide = 0;
	float PeekAlpha = 0.f;
	FVector PeekApplied = FVector::ZeroVector;

	// Vault
	bool bVaulting = false;
	float VaultTime = 0.f;
	FVector VaultStartFeet = FVector::ZeroVector;
	FVector VaultEndFeet = FVector::ZeroVector;

	// Roll
	bool bRolling = false;
	float RollEndTime = 0.f;
	FVector RollDir = FVector::ForwardVector;

	bool bAiming = false;
	bool bSprinting = false;
	bool bFireWasHeld = false;
	float LastFireTime = -100.f;
	float NextMeleeTime = 0.f;
	float LastHitMarkerTime = -100.f;
	bool bLastHitKill = false;
	float ShoulderSign = 1.f;

	bool TryEnterCover(const FVector& Dir);
	void ExitCover();
	bool HasCoverAt(float SideSign) const;
	bool TryVault();
	void TickVault(float DeltaSeconds);
	void TickCover(float DeltaSeconds, const FVector& WorldMove, bool bAimHeld);
	void StartRoll(const FVector& Dir);
	void UpdateCamera(float DeltaSeconds);
	void HandleFire(class APlayerController* PC, bool bFireHeld);
	void DoMelee();
	FVector CoverTangent() const { return FVector::CrossProduct(FVector::UpVector, CoverNormal); }
};
