#include "HFGameMode.h"
#include "HFArena.h"
#include "HFEnemyCharacter.h"
#include "HFHUD.h"
#include "HFPlayerCharacter.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AHFGameMode::AHFGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AHFPlayerCharacter::StaticClass();
	HUDClass = AHFHUD::StaticClass();
}

void AHFGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// The whole level is generated here so the project needs no map asset.
	Arena = GetWorld()->SpawnActor<AHFArena>(AHFArena::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (Arena)
	{
		Arena->Build();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlayerStartActor = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Arena->GetPlayerSpawn(), FRotator::ZeroRotator, Params);
	}
	SpawnEnvironment();
}

AActor* AHFGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return PlayerStartActor ? PlayerStartActor.Get() : Super::ChoosePlayerStart_Implementation(Player);
}

void AHFGameMode::SpawnEnvironment()
{
	UWorld* World = GetWorld();

	// Low, smoky dusk sun raking across the yard from behind the breach wall.
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 2000.f), FRotator(-16.f, 205.f, 0.f)))
	{
		ULightComponent* L = Sun->GetLightComponent();
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(5.f);
		L->SetLightColor(FLinearColor(1.f, 0.62f, 0.38f));
	}

	World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 1500.f), FRotator::ZeroRotator))
	{
		USkyLightComponent* L = Sky->GetLightComponent();
		L->SetMobility(EComponentMobility::Movable);
		L->bRealTimeCapture = true;
		L->SetIntensity(0.8f);
		L->MarkRenderStateDirty();
	}

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator))
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetFogDensity(0.045f);
		F->SetFogHeightFalloff(0.5f);
		F->SetFogInscatteringColor(FLinearColor(0.42f, 0.27f, 0.2f));
		F->SetVolumetricFog(true);
	}
}

void AHFGameMode::SetBanner(const FString& Text, float Duration)
{
	Banner = Text;
	BannerUntil = GetWorld()->GetTimeSeconds() + Duration;
}

int32 AHFGameMode::GetEnemiesRemaining() const
{
	int32 Count = SpawnQueue.Num();
	for (const TWeakObjectPtr<AHFEnemyCharacter>& E : Enemies)
	{
		if (E.IsValid() && !E->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void AHFGameMode::QueueWave(int32 Idx)
{
	static const int32 Table[NumWaves][3] = {
		{ 4, 0, 0 },
		{ 4, 3, 0 },
		{ 5, 4, 1 },
	};
	SpawnQueue.Reset();
	for (int32 i = 0; i < Table[Idx][0]; ++i) { SpawnQueue.Add(EHFEnemyKind::Drone); }
	for (int32 i = 0; i < Table[Idx][1]; ++i) { SpawnQueue.Add(EHFEnemyKind::Skitter); }
	for (int32 i = 0; i < Table[Idx][2]; ++i) { SpawnQueue.Add(EHFEnemyKind::Hauler); }
	// Shuffle so the kinds arrive mixed.
	for (int32 i = SpawnQueue.Num() - 1; i > 0; --i)
	{
		SpawnQueue.Swap(i, FMath::RandRange(0, i));
	}
}

void AHFGameMode::SpawnNext()
{
	if (!Arena || SpawnQueue.Num() == 0 || Arena->GetEnemySpawns().Num() == 0)
	{
		return;
	}
	const EHFEnemyKind Kind = SpawnQueue.Pop();
	const TArray<FVector>& Gates = Arena->GetEnemySpawns();
	FVector Loc = Gates[SpawnCursor++ % Gates.Num()];
	Loc.Y += FMath::FRandRange(-90.f, 90.f);
	Loc.Z = 135.f; // clear of the floor for the largest kind

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AHFEnemyCharacter* E = GetWorld()->SpawnActor<AHFEnemyCharacter>(AHFEnemyCharacter::StaticClass(), Loc, FRotator(0.f, 180.f, 0.f), Params))
	{
		E->InitKind(Kind, Arena);
		Enemies.Add(E);
	}
}

void AHFGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	AHFPlayerCharacter* Player = Cast<AHFPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));

	if (Phase == EPhase::Defeat)
	{
		if (Now >= RestartTime)
		{
			const FString MapName = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
			UGameplayStatics::OpenLevel(this, FName(*MapName));
		}
		return;
	}
	if (Player && Player->IsDead() && Phase != EPhase::Victory)
	{
		Phase = EPhase::Defeat;
		RestartTime = Now + 4.f;
		SetBanner(TEXT("YARD LOST"), 4.f);
		return;
	}

	switch (Phase)
	{
	case EPhase::Intermission:
		if (Now >= NextWaveTime)
		{
			QueueWave(WaveIdx);
			Phase = EPhase::Spawning;
			NextSpawnTime = Now;
			SetBanner(FString::Printf(TEXT("BREACH %d OF %d"), WaveIdx + 1, NumWaves), 3.f);
		}
		break;

	case EPhase::Spawning:
		if (SpawnQueue.Num() == 0)
		{
			Phase = EPhase::Fighting;
		}
		else if (Now >= NextSpawnTime)
		{
			SpawnNext();
			NextSpawnTime = Now + 0.8f;
		}
		break;

	case EPhase::Fighting:
		if (GetEnemiesRemaining() == 0)
		{
			Enemies.Reset();
			++WaveIdx;
			if (Player)
			{
				Player->RefillAmmo();
			}
			if (WaveIdx >= NumWaves)
			{
				Phase = EPhase::Victory;
				SetBanner(TEXT("YARD HELD"), 1.0e6f);
			}
			else
			{
				Phase = EPhase::Intermission;
				NextWaveTime = Now + 6.f;
				SetBanner(TEXT("BREACH SEALED - RESUPPLIED"), 3.f);
			}
		}
		break;

	default:
		break;
	}
}
