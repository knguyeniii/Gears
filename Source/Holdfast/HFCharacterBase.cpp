#include "HFCharacterBase.h"
#include "HFTracer.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AHFCharacterBase::AHFCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(38.f, 90.f);
	// Shots trace on Visibility; the chase camera must not collide with bodies.
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->NavAgentProps.bCanCrouch = true;
	Move->SetCrouchedHalfHeight(55.f);
	Move->MaxWalkSpeedCrouched = 260.f;
	Move->MaxAcceleration = 2600.f;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Capsule);
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -90.f));

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(VisualRoot);
	MuzzleLight->SetRelativeLocation(FVector(85.f, 22.f, 125.f));
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetAttenuationRadius(450.f);
	MuzzleLight->SetCastShadows(false);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.7f, 0.35f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	BaseMaterial = Mat.Object;
}

UStaticMeshComponent* AHFCharacterBase::AddPart(UStaticMesh* Mesh, const FVector& RelLoc, const FVector& Size, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(VisualRoot);
	C->SetRelativeLocation(RelLoc);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetCollisionProfileName(TEXT("NoCollision"));
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	}
	C->RegisterComponent();
	return C;
}

void AHFCharacterBase::BuildBody(const FLinearColor& Armor, const FLinearColor& Under, const FLinearColor& Accent)
{
	// Positions are relative to the feet; X is forward, Y is right.
	AddPart(CubeMesh, FVector(0.f, -14.f, 42.f), FVector(24.f, 22.f, 84.f), Under);   // legs
	AddPart(CubeMesh, FVector(0.f, 14.f, 42.f), FVector(24.f, 22.f, 84.f), Under);
	AddPart(CubeMesh, FVector(4.f, -14.f, 12.f), FVector(34.f, 24.f, 24.f), Armor);   // boots
	AddPart(CubeMesh, FVector(4.f, 14.f, 12.f), FVector(34.f, 24.f, 24.f), Armor);
	AddPart(CubeMesh, FVector(0.f, 0.f, 114.f), FVector(36.f, 58.f, 62.f), Armor);    // torso plate
	AddPart(CubeMesh, FVector(-20.f, 0.f, 118.f), FVector(14.f, 40.f, 44.f), Under);  // pack
	AddPart(CubeMesh, FVector(0.f, -40.f, 138.f), FVector(30.f, 26.f, 24.f), Armor);  // shoulders
	AddPart(CubeMesh, FVector(0.f, 40.f, 138.f), FVector(30.f, 26.f, 24.f), Armor);
	AddPart(CubeMesh, FVector(8.f, -40.f, 108.f), FVector(18.f, 16.f, 44.f), Under);  // arms
	AddPart(CubeMesh, FVector(18.f, 36.f, 112.f), FVector(40.f, 16.f, 18.f), Under);
	AddPart(SphereMesh, FVector(0.f, 0.f, 162.f), FVector(28.f, 28.f, 30.f), Under);  // head
	AddPart(CubeMesh, FVector(13.f, 0.f, 163.f), FVector(6.f, 20.f, 7.f), Accent);    // visor
	AddPart(CubeMesh, FVector(19.f, 0.f, 122.f), FVector(4.f, 22.f, 10.f), Accent);   // chest light
	AddPart(CubeMesh, FVector(48.f, 22.f, 122.f), FVector(74.f, 9.f, 14.f), FLinearColor(0.03f, 0.03f, 0.035f)); // weapon
	AddPart(CubeMesh, FVector(30.f, 22.f, 110.f), FVector(12.f, 8.f, 20.f), FLinearColor(0.03f, 0.03f, 0.035f));
}

void AHFCharacterBase::AddWeapon(const FHFWeaponStats& Stats, int32 Reserve)
{
	FHFWeapon W;
	W.Stats = Stats;
	W.Ammo = Stats.MagSize;
	W.Reserve = Reserve;
	Weapons.Add(W);
}

void AHFCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();

	// Keep the primitive body glued to the feet as the capsule crouches.
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	LowPoseAlpha = FMath::FInterpTo(LowPoseAlpha, (bIsCrouched || bForceLowPose) ? 1.f : 0.f, DeltaSeconds, 12.f);
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	VisualRoot->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Lerp(1.f, 0.62f, LowPoseAlpha)));

	if (bDead)
	{
		const float A = FMath::Clamp((Now - DeathTime) / 0.35f, 0.f, 1.f);
		VisualRoot->SetRelativeRotation(FRotator(85.f * A, 0.f, 0.f));
		MuzzleLight->SetIntensity(0.f);
		return;
	}

	if (Now >= MuzzleOffTime)
	{
		MuzzleLight->SetIntensity(0.f);
	}

	if (Weapons.IsValidIndex(WeaponIdx))
	{
		FHFWeapon& W = Weapons[WeaponIdx];
		if (W.bReloading && Now >= W.ReloadEndTime)
		{
			const int32 Take = FMath::Min(W.Stats.MagSize - W.Ammo, W.Reserve);
			W.Ammo += Take;
			W.Reserve -= Take;
			W.bReloading = false;
		}
	}
}

void AHFCharacterBase::StartReload()
{
	if (!Weapons.IsValidIndex(WeaponIdx))
	{
		return;
	}
	FHFWeapon& W = Weapons[WeaponIdx];
	if (W.bReloading || W.Ammo >= W.Stats.MagSize || W.Reserve <= 0)
	{
		return;
	}
	W.bReloading = true;
	W.ReloadEndTime = GetWorld()->GetTimeSeconds() + W.Stats.ReloadTime;
}

bool AHFCharacterBase::FireShot(const FVector& Muzzle, const FVector& AimPoint, float SpreadMul)
{
	if (bDead || !Weapons.IsValidIndex(WeaponIdx))
	{
		return false;
	}
	FHFWeapon& W = Weapons[WeaponIdx];
	const float Now = GetWorld()->GetTimeSeconds();
	if (W.bReloading || Now < W.NextFireTime)
	{
		return false;
	}
	if (W.Ammo <= 0)
	{
		StartReload();
		return false;
	}

	--W.Ammo;
	W.NextFireTime = Now + W.Stats.FireInterval;

	const FVector BaseDir = (AimPoint - Muzzle).GetSafeNormal();
	const float HalfAngle = FMath::DegreesToRadians(FMath::Max(0.01f, W.Stats.SpreadDeg * SpreadMul));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HFShot), false, this);

	for (int32 i = 0; i < W.Stats.Pellets; ++i)
	{
		const FVector Dir = FMath::VRandCone(BaseDir, HalfAngle);
		const FVector End = Muzzle + Dir * W.Stats.Range;
		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, End, ECC_Visibility, Params);
		AHFTracer::Spawn(GetWorld(), Muzzle, bHit ? Hit.ImpactPoint : End, TracerColor);

		AHFCharacterBase* Victim = bHit ? Cast<AHFCharacterBase>(Hit.GetActor()) : nullptr;
		if (!Victim || Victim->IsDead() || Victim->GetTeamId() == TeamId)
		{
			continue;
		}

		float Damage = W.Stats.Damage;
		if (Hit.Distance > W.Stats.FalloffStart)
		{
			const float T = (Hit.Distance - W.Stats.FalloffStart) / FMath::Max(1.f, W.Stats.Range - W.Stats.FalloffStart);
			Damage *= FMath::Lerp(1.f, 0.25f, FMath::Clamp(T, 0.f, 1.f));
		}
		const float VictimHalfHeight = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const bool bHead = Hit.ImpactPoint.Z > Victim->GetActorLocation().Z + VictimHalfHeight * 0.55f;
		if (bHead)
		{
			Damage *= 1.8f;
		}
		Victim->ReceiveHit(Damage, this, Muzzle);
		OnDealtDamage(Victim->IsDead(), bHead);
	}

	MuzzleLight->SetIntensity(22000.f);
	MuzzleOffTime = Now + 0.05f;
	if (W.Ammo <= 0)
	{
		StartReload();
	}
	return true;
}

void AHFCharacterBase::ReceiveHit(float Damage, AHFCharacterBase* From, const FVector& FromLocation)
{
	if (bDead)
	{
		return;
	}
	Health -= Damage;
	LastDamageTime = GetWorld()->GetTimeSeconds();
	if (Health <= 0.f)
	{
		Die(From);
	}
}

void AHFCharacterBase::Die(AHFCharacterBase* Killer)
{
	bDead = true;
	Health = 0.f;
	DeathTime = GetWorld()->GetTimeSeconds();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
