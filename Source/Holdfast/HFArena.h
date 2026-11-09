#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFTypes.h"
#include "HFArena.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Builds the freight-yard battle area entirely in code (no map assets needed),
 * and owns the cover slots and the walkability grid the AI paths on.
 */
UCLASS()
class HOLDFAST_API AHFArena : public AActor
{
	GENERATED_BODY()

public:
	AHFArena();

	void Build();

	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;

	int32 FindBestSlot(const FVector& From, const FVector& Threat, const AActor* For, float PreferredRange, int32 ExcludeIdx) const;
	bool SlotCoversFrom(int32 Idx, const FVector& Threat) const;
	void ClaimSlot(int32 Idx, AActor* By);
	void ReleaseSlot(int32 Idx, const AActor* By);
	FVector GetSlotLocation(int32 Idx) const { return Slots.IsValidIndex(Idx) ? Slots[Idx].Location : FVector::ZeroVector; }

	const TArray<FVector>& GetEnemySpawns() const { return EnemySpawns; }
	FVector GetPlayerSpawn() const { return FVector(-1750.f, 0.f, 100.f); }

	static const FName CoverTag;
	static const FName LowCoverTag;

private:
	static constexpr int32 GX = 40;
	static constexpr int32 GY = 30;
	static constexpr float Cell = 100.f;
	static constexpr float MinX = -2000.f;
	static constexpr float MinY = -1500.f;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	TArray<uint8> Blocked;
	TArray<FHFCoverSlot> Slots;
	TArray<FVector> EnemySpawns;

	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide = true);
	void AddCover(const FVector2D& CenterXY, const FVector& Size, bool bLow, const FLinearColor& Color);
	void AddLamp(const FVector& Base, const FLinearColor& Color);

	FIntPoint ToCell(const FVector& P) const;
	FVector CellCenter(int32 X, int32 Y) const;
	bool IsBlocked(int32 X, int32 Y) const;
	FIntPoint NearestFree(FIntPoint C) const;
	bool GridLineClear(const FVector& A, const FVector& B) const;
};
