#pragma once

#include "CoreMinimal.h"
#include "HFCharacterBase.h"
#include "HFEnemyCharacter.generated.h"

class AHFArena;

/** Rogue yard machine. One class, three behaviours selected by kind. */
UCLASS()
class HOLDFAST_API AHFEnemyCharacter : public AHFCharacterBase
{
	GENERATED_BODY()

public:
	AHFEnemyCharacter();

	void InitKind(EHFEnemyKind InKind, AHFArena* InArena);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void Die(AHFCharacterBase* Killer) override;

private:
	enum class EState : uint8 { MoveToCover, InCover, Peek, Chase };

	EHFEnemyKind Kind = EHFEnemyKind::Drone;
	EState State = EState::Chase;
	TWeakObjectPtr<AHFArena> Arena;

	TArray<FVector> Path;
	int32 PathIdx = 0;
	int32 SlotIdx = INDEX_NONE;
	float StateEndTime = 0.f;
	float NextRepathTime = 0.f;
	float NextLosTime = 0.f;
	bool bHasLos = false;
	int32 Cycles = 0;
	int32 CyclesBeforeMove = 2;
	float PreferredRange = 1100.f;
	int32 BurstLeft = 0;
	float NextBurstTime = 0.f;
	float NextMeleeTime = 0.f;
	float StuckTime = 0.f;

	void PickCover(const FVector& Threat);
	bool FollowPath();
	void PathTo(const FVector& Goal);
	void TryBurst(const FVector& TargetPoint, float SpreadMul, int32 MinShots, int32 MaxShots);
	void TickDrone(AHFCharacterBase* Target, float Dist, float Now);
	void TickSkitter(AHFCharacterBase* Target, float Dist, float Now);
	void TickHauler(AHFCharacterBase* Target, float Dist, float Now);
};
