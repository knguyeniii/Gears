#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HFTypes.h"
#include "HFGameMode.generated.h"

class AHFArena;
class AHFEnemyCharacter;

/** Builds the yard and lighting at startup, then runs the wave encounter. */
UCLASS()
class HOLDFAST_API AHFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHFGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Tick(float DeltaSeconds) override;

	int32 GetWaveNumber() const { return FMath::Min(WaveIdx + 1, NumWaves); }
	int32 GetNumWaves() const { return NumWaves; }
	int32 GetEnemiesRemaining() const;
	const FString& GetBanner() const { return Banner; }
	float GetBannerUntil() const { return BannerUntil; }
	bool IsVictory() const { return Phase == EPhase::Victory; }
	bool IsDefeat() const { return Phase == EPhase::Defeat; }

private:
	enum class EPhase : uint8 { Intermission, Spawning, Fighting, Victory, Defeat };
	static constexpr int32 NumWaves = 3;

	UPROPERTY()
	TObjectPtr<AHFArena> Arena;
	UPROPERTY()
	TObjectPtr<AActor> PlayerStartActor;

	EPhase Phase = EPhase::Intermission;
	int32 WaveIdx = 0;
	float NextWaveTime = 4.f;
	float NextSpawnTime = 0.f;
	float RestartTime = 0.f;
	int32 SpawnCursor = 0;
	TArray<EHFEnemyKind> SpawnQueue;
	TArray<TWeakObjectPtr<AHFEnemyCharacter>> Enemies;
	FString Banner;
	float BannerUntil = 0.f;

	void SpawnEnvironment();
	void QueueWave(int32 Idx);
	void SpawnNext();
	void SetBanner(const FString& Text, float Duration);
};
