#include "HFEnemyCharacter.h"
#include "HFArena.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AHFEnemyCharacter::AHFEnemyCharacter()
{
	TeamId = 1;
	TracerColor = FLinearColor(1.f, 0.2f, 0.1f);
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.3f, 0.15f));
}

void AHFEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (!GetController())
	{
		SpawnDefaultController();
	}
}

void AHFEnemyCharacter::InitKind(EHFEnemyKind InKind, AHFArena* InArena)
{
	Kind = InKind;
	Arena = InArena;

	const FLinearColor Eye(0.25f, 1.f, 0.3f);
	FHFWeaponStats Gun;
	UCharacterMovementComponent* Move = GetCharacterMovement();

	switch (Kind)
	{
	case EHFEnemyKind::Drone:
		MaxHealth = 100.f;
		Move->MaxWalkSpeed = 400.f;
		Gun.Name = TEXT("Rivet gun");
		Gun.Damage = 6.f;
		Gun.FireInterval = 0.14f;
		Gun.MagSize = 24;
		Gun.ReloadTime = 2.2f;
		Gun.SpreadDeg = 3.2f;
		AddWeapon(Gun, 1000000);
		BuildBody(FLinearColor(0.32f, 0.11f, 0.05f), FLinearColor(0.06f, 0.05f, 0.05f), Eye);
		State = EState::Peek;
		StateEndTime = 0.f;
		break;

	case EHFEnemyKind::Skitter:
		MaxHealth = 55.f;
		SetActorScale3D(FVector(0.8f));
		Move->MaxWalkSpeed = 640.f;
		BuildBody(FLinearColor(0.45f, 0.36f, 0.05f), FLinearColor(0.05f, 0.05f, 0.05f), FLinearColor(1.f, 0.1f, 0.1f));
		State = EState::Chase;
		break;

	case EHFEnemyKind::Hauler:
		MaxHealth = 900.f;
		SetActorScale3D(FVector(1.4f));
		Move->MaxWalkSpeed = 210.f;
		Gun.Name = TEXT("Chain driver");
		Gun.Damage = 7.f;
		Gun.FireInterval = 0.08f;
		Gun.MagSize = 80;
		Gun.ReloadTime = 3.5f;
		Gun.SpreadDeg = 4.5f;
		AddWeapon(Gun, 1000000);
		BuildBody(FLinearColor(0.12f, 0.12f, 0.13f), FLinearColor(0.3f, 0.1f, 0.04f), Eye);
		State = EState::Chase;
		break;
	}
	Health = MaxHealth;
}

void AHFEnemyCharacter::Die(AHFCharacterBase* Killer)
{
	Super::Die(Killer);
	if (Arena.IsValid())
	{
		Arena->ReleaseSlot(SlotIdx, this);
	}
	SlotIdx = INDEX_NONE;
	DetachFromControllerPendingDestroy();
	SetLifeSpan(5.f);
}

void AHFEnemyCharacter::PathTo(const FVector& Goal)
{
	Path.Reset();
	PathIdx = 0;
	if (Arena.IsValid())
	{
		Arena->FindPath(GetActorLocation(), Goal, Path);
	}
}

bool AHFEnemyCharacter::FollowPath()
{
	while (PathIdx < Path.Num())
	{
		FVector Delta = Path[PathIdx] - GetActorLocation();
		Delta.Z = 0.f;
		if (Delta.SizeSquared() < 45.f * 45.f)
		{
			++PathIdx;
			continue;
		}
		AddMovementInput(Delta.GetSafeNormal());
		return true;
	}
	return false;
}

void AHFEnemyCharacter::PickCover(const FVector& Threat)
{
	if (!Arena.IsValid())
	{
		return;
	}
	const int32 Old = SlotIdx;
	const int32 New = Arena->FindBestSlot(GetActorLocation(), Threat, this, PreferredRange, Old);
	if (New == INDEX_NONE)
	{
		// Nowhere better: stand and fight for a moment.
		UnCrouch();
		State = EState::Peek;
		StateEndTime = GetWorld()->GetTimeSeconds() + 2.f;
		Cycles = 0;
		return;
	}
	Arena->ReleaseSlot(Old, this);
	Arena->ClaimSlot(New, this);
	SlotIdx = New;
	PathTo(Arena->GetSlotLocation(New));
	UnCrouch();
	State = EState::MoveToCover;
	Cycles = 0;
	CyclesBeforeMove = FMath::RandRange(2, 3);
	StuckTime = 0.f;
	// Each relocation presses closer.
	PreferredRange = FMath::Max(450.f, PreferredRange - 250.f);
}

void AHFEnemyCharacter::TryBurst(const FVector& TargetPoint, float SpreadMul, int32 MinShots, int32 MaxShots)
{
	if (bIsCrouched)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (BurstLeft <= 0)
	{
		if (Now < NextBurstTime)
		{
			return;
		}
		BurstLeft = FMath::RandRange(MinShots, MaxShots);
	}
	const FVector Fwd = (TargetPoint - GetActorLocation()).GetSafeNormal2D();
	const float Scale = static_cast<float>(GetActorScale3D().Z);
	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 40.f * Scale) + Fwd * 50.f * Scale;
	if (FireShot(Muzzle, TargetPoint, SpreadMul) && --BurstLeft <= 0)
	{
		NextBurstTime = Now + FMath::FRandRange(0.7f, 1.4f);
	}
}

void AHFEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead)
	{
		return;
	}

	AHFCharacterBase* Target = Cast<AHFCharacterBase>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Target || Target->IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Dist = ToTarget.Size2D();

	if (Now >= NextLosTime)
	{
		NextLosTime = Now + 0.15f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HFLos), false, this);
		const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 45.f * GetActorScale3D().Z);
		bHasLos = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation(), ECC_Visibility, Params) && Hit.GetActor() == Target;
	}

	switch (Kind)
	{
	case EHFEnemyKind::Drone:
		TickDrone(Target, Dist, Now);
		break;
	case EHFEnemyKind::Skitter:
		TickSkitter(Target, Dist, Now);
		break;
	case EHFEnemyKind::Hauler:
		TickHauler(Target, Dist, Now);
		break;
	}

	// Face the target when it matters, otherwise the direction of travel.
	FRotator Want = GetActorRotation();
	if (bHasLos || State == EState::InCover || State == EState::Peek)
	{
		Want = FRotator(0.f, ToTarget.Rotation().Yaw, 0.f);
	}
	else if (GetVelocity().SizeSquared2D() > 400.f)
	{
		Want = FRotator(0.f, GetVelocity().Rotation().Yaw, 0.f);
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 9.f));
}

void AHFEnemyCharacter::TickDrone(AHFCharacterBase* Target, float Dist, float Now)
{
	const FVector Threat = Target->GetActorLocation();
	const FVector AimAt = Threat + FVector(0.f, 0.f, 10.f);
	const bool bSlotStillGood = Arena.IsValid() && Arena->SlotCoversFrom(SlotIdx, Threat);

	switch (State)
	{
	case EState::MoveToCover:
		if (!FollowPath())
		{
			State = EState::InCover;
			StateEndTime = Now + FMath::FRandRange(0.8f, 2.f);
			Crouch();
			break;
		}
		if (bHasLos && Dist < 1800.f)
		{
			TryBurst(AimAt, 2.f, 2, 4); // suppressing fire on the move
		}
		// Blocked by another machine or the player: pick somewhere else.
		StuckTime = GetVelocity().SizeSquared2D() < 30.f * 30.f ? StuckTime + GetWorld()->GetDeltaSeconds() : 0.f;
		if (StuckTime > 1.2f || (Now >= NextRepathTime && !bSlotStillGood))
		{
			NextRepathTime = Now + 1.f;
			PickCover(Threat);
		}
		break;

	case EState::InCover:
		if (!bSlotStillGood || Dist < 300.f)
		{
			PickCover(Threat); // flanked or overrun
		}
		else if (Now >= StateEndTime)
		{
			State = EState::Peek;
			StateEndTime = Now + FMath::FRandRange(1.5f, 3.f);
			UnCrouch();
		}
		break;

	case EState::Peek:
		if (bHasLos)
		{
			TryBurst(AimAt, 1.f, 3, 6);
		}
		if (Now >= StateEndTime)
		{
			++Cycles;
			if (SlotIdx == INDEX_NONE || Cycles >= CyclesBeforeMove || !bSlotStillGood || Dist > 2000.f)
			{
				PickCover(Threat);
			}
			else
			{
				State = EState::InCover;
				StateEndTime = Now + FMath::FRandRange(1.f, 2.2f);
				Crouch();
			}
		}
		break;

	default:
		PickCover(Threat);
		break;
	}
}

void AHFEnemyCharacter::TickSkitter(AHFCharacterBase* Target, float Dist, float Now)
{
	const FVector Threat = Target->GetActorLocation();
	if (Dist < 140.f * FMath::Max(1.f, static_cast<float>(GetActorScale3D().X)))
	{
		if (Now >= NextMeleeTime)
		{
			NextMeleeTime = Now + 0.9f;
			Target->ReceiveHit(22.f, this, GetActorLocation());
		}
		return;
	}
	if (bHasLos && Dist < 450.f)
	{
		AddMovementInput((Threat - GetActorLocation()).GetSafeNormal2D());
		return;
	}
	if (Now >= NextRepathTime)
	{
		NextRepathTime = Now + 0.35f;
		PathTo(Threat);
	}
	FollowPath();
}

void AHFEnemyCharacter::TickHauler(AHFCharacterBase* Target, float Dist, float Now)
{
	const FVector Threat = Target->GetActorLocation();
	if (bHasLos)
	{
		TryBurst(Threat + FVector(0.f, 0.f, 10.f), 1.f, 14, 26);
	}
	if (!bHasLos || Dist > 900.f)
	{
		if (Now >= NextRepathTime)
		{
			NextRepathTime = Now + 0.6f;
			PathTo(Threat);
		}
		FollowPath();
	}
}
