#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFTracer.generated.h"

class UStaticMeshComponent;

/** Short-lived streak drawn along a shot. */
UCLASS()
class HOLDFAST_API AHFTracer : public AActor
{
	GENERATED_BODY()

public:
	AHFTracer();

	static void Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;
};
