#include "HFArena.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

const FName AHFArena::CoverTag(TEXT("Cover"));
const FName AHFArena::LowCoverTag(TEXT("CoverLow"));

AHFArena::AHFArena()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	BaseMaterial = Mat.Object;
}

UStaticMeshComponent* AHFArena::AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(CubeMesh);
	C->SetupAttachment(Root);
	C->SetRelativeLocation(Center);
	C->SetRelativeScale3D(Size / 100.f); // engine cube is 100uu
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	}
	C->RegisterComponent();
	return C;
}

void AHFArena::AddCover(const FVector2D& CenterXY, const FVector& Size, bool bLow, const FLinearColor& Color)
{
	UStaticMeshComponent* C = AddBox(FVector(CenterXY.X, CenterXY.Y, Size.Z * 0.5f), Size, Color);
	C->ComponentTags.Add(CoverTag);
	if (bLow)
	{
		C->ComponentTags.Add(LowCoverTag);
		// Hazard strip along the top edge so low cover reads at a glance.
		AddBox(FVector(CenterXY.X, CenterXY.Y, Size.Z + 3.f), FVector(Size.X + 4.f, Size.Y + 4.f, 6.f), FLinearColor(0.85f, 0.55f, 0.05f), false);
	}

	// Mark the walkability grid, inflated so capsules keep clear of the faces.
	const float Inflate = 45.f;
	for (int32 Y = 0; Y < GY; ++Y)
	{
		for (int32 X = 0; X < GX; ++X)
		{
			const FVector P = CellCenter(X, Y);
			if (FMath::Abs(P.X - CenterXY.X) <= Size.X * 0.5f + Inflate && FMath::Abs(P.Y - CenterXY.Y) <= Size.Y * 0.5f + Inflate)
			{
				Blocked[Y * GX + X] = 1;
			}
		}
	}

	if (!bLow)
	{
		return;
	}

	// AI slots along both long faces.
	const bool bLongY = Size.Y >= Size.X;
	const float Long = bLongY ? Size.Y : Size.X;
	const float Thin = bLongY ? Size.X : Size.Y;
	const int32 Count = FMath::Max(1, FMath::FloorToInt((Long - 80.f) / 100.f) + 1);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const float Along = (i - (Count - 1) * 0.5f) * 100.f;
			const float Out = Side * (Thin * 0.5f + 55.f);
			FHFCoverSlot Slot;
			Slot.Location = bLongY ? FVector(CenterXY.X + Out, CenterXY.Y + Along, 0.f) : FVector(CenterXY.X + Along, CenterXY.Y + Out, 0.f);
			Slot.Normal = bLongY ? FVector(Side, 0.f, 0.f) : FVector(0.f, Side, 0.f);
			Slots.Add(Slot);
		}
	}
}

void AHFArena::AddLamp(const FVector& Base, const FLinearColor& Color)
{
	AddBox(Base + FVector(0.f, 0.f, 225.f), FVector(14.f, 14.f, 450.f), FLinearColor(0.05f, 0.05f, 0.05f), false);
	AddBox(Base + FVector(0.f, 0.f, 455.f), FVector(60.f, 30.f, 12.f), FLinearColor(0.9f, 0.85f, 0.7f), false);

	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(Base + FVector(0.f, 0.f, 420.f));
	L->SetIntensity(9000.f);
	L->SetAttenuationRadius(1300.f);
	L->SetLightColor(Color);
	L->RegisterComponent();
}

void AHFArena::Build()
{
	Blocked.Init(0, GX * GY);
	Slots.Reset();
	EnemySpawns.Reset();

	const FLinearColor Concrete(0.085f, 0.085f, 0.09f);
	const FLinearColor Wall(0.14f, 0.13f, 0.12f);
	const FLinearColor Barrier(0.36f, 0.35f, 0.32f);
	const FLinearColor ContainerRed(0.30f, 0.09f, 0.05f);
	const FLinearColor ContainerTeal(0.04f, 0.17f, 0.22f);
	const FLinearColor Steel(0.22f, 0.22f, 0.24f);

	// Ground slab and perimeter.
	AddBox(FVector(0.f, 0.f, -50.f), FVector(4400.f, 3400.f, 100.f), Concrete);
	AddBox(FVector(-2050.f, 0.f, 250.f), FVector(100.f, 3200.f, 500.f), Wall);
	AddBox(FVector(2050.f, 0.f, 250.f), FVector(100.f, 3200.f, 500.f), Wall);
	AddBox(FVector(0.f, -1550.f, 250.f), FVector(4200.f, 100.f, 500.f), Wall);
	AddBox(FVector(0.f, 1550.f, 250.f), FVector(4200.f, 100.f, 500.f), Wall);

	// Rail lines running the length of the yard (visual only).
	for (float Y : { -190.f, -110.f, 110.f, 190.f })
	{
		AddBox(FVector(0.f, Y, 4.f), FVector(4000.f, 10.f, 8.f), Steel, false);
	}
	for (float X = -1900.f; X <= 1900.f; X += 200.f)
	{
		AddBox(FVector(X, -150.f, 1.5f), FVector(26.f, 130.f, 3.f), FLinearColor(0.05f, 0.04f, 0.03f), false);
		AddBox(FVector(X, 150.f, 1.5f), FVector(26.f, 130.f, 3.f), FLinearColor(0.05f, 0.04f, 0.03f), false);
	}

	// Breach gates on the east wall, where the machines come through.
	for (float Y : { -900.f, 0.f, 900.f })
	{
		AddBox(FVector(1997.f, Y, 160.f), FVector(8.f, 320.f, 320.f), FLinearColor(0.01f, 0.01f, 0.01f), false);
		AddBox(FVector(1994.f, Y, 330.f), FVector(10.f, 340.f, 20.f), FLinearColor(0.7f, 0.08f, 0.02f), false);
		EnemySpawns.Add(FVector(1850.f, Y, 0.f));
	}

	// Staggered rows of low barricades.
	const FVector LowSize(60.f, 300.f, 110.f);
	const float RowX[] = { -1300.f, -700.f, -100.f, 500.f, 1100.f };
	for (int32 Row = 0; Row < 5; ++Row)
	{
		if (Row % 2 == 0)
		{
			for (float Y : { -600.f, 0.f, 600.f })
			{
				AddCover(FVector2D(RowX[Row], Y), LowSize, true, Barrier);
			}
		}
		else
		{
			for (float Y : { -900.f, -300.f, 300.f, 900.f })
			{
				AddCover(FVector2D(RowX[Row], Y), LowSize, true, Barrier);
			}
		}
	}

	// Tall cover: freight containers on the flanks, gantry pillars mid-yard.
	AddCover(FVector2D(200.f, -1250.f), FVector(600.f, 250.f, 260.f), false, ContainerRed);
	AddCover(FVector2D(200.f, 1250.f), FVector(600.f, 250.f, 260.f), false, ContainerTeal);
	AddCover(FVector2D(-1000.f, -1250.f), FVector(600.f, 250.f, 260.f), false, ContainerTeal);
	AddCover(FVector2D(-1000.f, 1250.f), FVector(600.f, 250.f, 260.f), false, ContainerRed);
	for (const FVector2D& P : { FVector2D(-1000.f, -300.f), FVector2D(-1000.f, 300.f), FVector2D(800.f, -600.f), FVector2D(800.f, 600.f) })
	{
		AddCover(P, FVector(130.f, 130.f, 480.f), false, Steel);
	}
	// Gantry beams tying the pillar pairs together overhead.
	AddBox(FVector(-1000.f, 0.f, 500.f), FVector(90.f, 760.f, 50.f), Steel, false);
	AddBox(FVector(800.f, 0.f, 500.f), FVector(90.f, 1360.f, 50.f), Steel, false);

	AddLamp(FVector(-1600.f, -1450.f, 0.f), FLinearColor(1.f, 0.75f, 0.45f));
	AddLamp(FVector(-1600.f, 1450.f, 0.f), FLinearColor(1.f, 0.75f, 0.45f));
	AddLamp(FVector(-300.f, -1450.f, 0.f), FLinearColor(1.f, 0.75f, 0.45f));
	AddLamp(FVector(-300.f, 1450.f, 0.f), FLinearColor(1.f, 0.75f, 0.45f));
	AddLamp(FVector(1500.f, -1450.f, 0.f), FLinearColor(1.f, 0.25f, 0.12f));
	AddLamp(FVector(1500.f, 1450.f, 0.f), FLinearColor(1.f, 0.25f, 0.12f));
}

// ---------------------------------------------------------------- cover slots

bool AHFArena::SlotCoversFrom(int32 Idx, const FVector& Threat) const
{
	if (!Slots.IsValidIndex(Idx))
	{
		return false;
	}
	const FVector ToThreat = (Threat - Slots[Idx].Location).GetSafeNormal2D();
	return FVector::DotProduct(Slots[Idx].Normal, ToThreat) < -0.45f;
}

int32 AHFArena::FindBestSlot(const FVector& From, const FVector& Threat, const AActor* For, float PreferredRange, int32 ExcludeIdx) const
{
	int32 Best = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FHFCoverSlot& S = Slots[i];
		if (i == ExcludeIdx || (S.Claimant.IsValid() && S.Claimant.Get() != For) || !SlotCoversFrom(i, Threat))
		{
			continue;
		}
		const float ThreatDist = FVector::Dist2D(S.Location, Threat);
		if (ThreatDist < 300.f)
		{
			continue;
		}
		const float Score = FVector::Dist2D(S.Location, From) * 0.6f + FMath::Abs(ThreatDist - PreferredRange) + FMath::FRandRange(0.f, 150.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = i;
		}
	}
	return Best;
}

void AHFArena::ClaimSlot(int32 Idx, AActor* By)
{
	if (Slots.IsValidIndex(Idx))
	{
		Slots[Idx].Claimant = By;
	}
}

void AHFArena::ReleaseSlot(int32 Idx, const AActor* By)
{
	if (Slots.IsValidIndex(Idx) && Slots[Idx].Claimant.Get() == By)
	{
		Slots[Idx].Claimant = nullptr;
	}
}

// ------------------------------------------------------------- grid + A*

FIntPoint AHFArena::ToCell(const FVector& P) const
{
	return FIntPoint(
		FMath::Clamp(FMath::FloorToInt((P.X - MinX) / Cell), 0, GX - 1),
		FMath::Clamp(FMath::FloorToInt((P.Y - MinY) / Cell), 0, GY - 1));
}

FVector AHFArena::CellCenter(int32 X, int32 Y) const
{
	return FVector(MinX + (X + 0.5f) * Cell, MinY + (Y + 0.5f) * Cell, 0.f);
}

bool AHFArena::IsBlocked(int32 X, int32 Y) const
{
	return X < 0 || Y < 0 || X >= GX || Y >= GY || Blocked[Y * GX + X] != 0;
}

FIntPoint AHFArena::NearestFree(FIntPoint C) const
{
	if (!IsBlocked(C.X, C.Y))
	{
		return C;
	}
	for (int32 R = 1; R <= 6; ++R)
	{
		FIntPoint Best(-1, -1);
		int32 BestD = MAX_int32;
		for (int32 DY = -R; DY <= R; ++DY)
		{
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || IsBlocked(C.X + DX, C.Y + DY))
				{
					continue;
				}
				const int32 D = DX * DX + DY * DY;
				if (D < BestD)
				{
					BestD = D;
					Best = FIntPoint(C.X + DX, C.Y + DY);
				}
			}
		}
		if (Best.X >= 0)
		{
			return Best;
		}
	}
	return C;
}

bool AHFArena::GridLineClear(const FVector& A, const FVector& B) const
{
	const float Len = FVector::Dist2D(A, B);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / 25.f));
	for (int32 i = 0; i <= Steps; ++i)
	{
		const FIntPoint C = ToCell(FMath::Lerp(A, B, static_cast<float>(i) / Steps));
		if (IsBlocked(C.X, C.Y))
		{
			return false;
		}
	}
	return true;
}

bool AHFArena::FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	OutPath.Reset();
	if (Blocked.Num() != GX * GY)
	{
		return false;
	}

	const FIntPoint S = NearestFree(ToCell(From));
	const FIntPoint G = NearestFree(ToCell(To));
	const int32 N = GX * GY;
	const int32 SI = S.Y * GX + S.X;
	const int32 GI = G.Y * GX + G.X;

	TArray<float> Cost;
	Cost.Init(TNumericLimits<float>::Max(), N);
	TArray<int32> Parent;
	Parent.Init(INDEX_NONE, N);
	TArray<uint8> Closed;
	Closed.Init(0, N);
	TArray<int32> Open;

	auto Heuristic = [&](int32 I) { return static_cast<float>(FVector2D::Distance(FVector2D(I % GX, I / GX), FVector2D(G.X, G.Y))); };

	Cost[SI] = 0.f;
	Open.Add(SI);
	bool bFound = false;

	while (Open.Num() > 0)
	{
		int32 BestOpen = 0;
		float BestF = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Open.Num(); ++i)
		{
			const float F = Cost[Open[i]] + Heuristic(Open[i]);
			if (F < BestF)
			{
				BestF = F;
				BestOpen = i;
			}
		}
		const int32 Cur = Open[BestOpen];
		Open.RemoveAtSwap(BestOpen);
		if (Cur == GI)
		{
			bFound = true;
			break;
		}
		if (Closed[Cur])
		{
			continue;
		}
		Closed[Cur] = 1;

		const int32 CX = Cur % GX;
		const int32 CY = Cur / GX;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX == 0 && DY == 0) || IsBlocked(CX + DX, CY + DY))
				{
					continue;
				}
				// No cutting corners around blocked cells.
				if (DX != 0 && DY != 0 && (IsBlocked(CX + DX, CY) || IsBlocked(CX, CY + DY)))
				{
					continue;
				}
				const int32 NI = (CY + DY) * GX + (CX + DX);
				const float NewCost = Cost[Cur] + ((DX != 0 && DY != 0) ? 1.4142f : 1.f);
				if (!Closed[NI] && NewCost < Cost[NI])
				{
					Cost[NI] = NewCost;
					Parent[NI] = Cur;
					Open.Add(NI);
				}
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	TArray<FVector> Raw;
	for (int32 I = GI; I != INDEX_NONE; I = Parent[I])
	{
		Raw.Insert(CellCenter(I % GX, I / GX), 0);
	}

	// Greedy string-pull so agents do not walk the grid staircase.
	FVector Anchor = Raw[0];
	int32 i = 0;
	while (i < Raw.Num() - 1)
	{
		int32 Far = i + 1;
		for (int32 j = Raw.Num() - 1; j > i + 1; --j)
		{
			if (GridLineClear(Anchor, Raw[j]))
			{
				Far = j;
				break;
			}
		}
		OutPath.Add(Raw[Far]);
		Anchor = Raw[Far];
		i = Far;
	}
	if (OutPath.Num() == 0)
	{
		OutPath.Add(Raw.Last());
	}

	// Final hop to the exact goal (cover slots sit inside inflated cells).
	OutPath.Add(FVector(To.X, To.Y, 0.f));
	return true;
}
