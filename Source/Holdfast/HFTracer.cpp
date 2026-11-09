#include "HFTracer.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AHFTracer::AHFTracer()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 0.07f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("NoCollision"));
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetMaterial(0, Mat.Object);
}

void AHFTracer::Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color)
{
	if (!World)
	{
		return;
	}
	const FVector Delta = End - Start;
	const float Len = Delta.Size();
	if (Len < 1.f)
	{
		return;
	}
	AHFTracer* T = World->SpawnActor<AHFTracer>(AHFTracer::StaticClass(), (Start + End) * 0.5f, Delta.Rotation());
	if (!T)
	{
		return;
	}
	T->Mesh->SetWorldScale3D(FVector(Len / 100.f, 0.02f, 0.02f));
	if (UMaterialInstanceDynamic* MID = T->Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
