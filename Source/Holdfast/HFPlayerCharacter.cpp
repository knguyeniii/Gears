#include "HFPlayerCharacter.h"
#include "HFArena.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

namespace
{
	const float WalkSpeed = 460.f;
	const float SprintSpeed = 720.f;
	const float AimSpeed = 240.f;
	const float CoverSpeed = 260.f;
	const float RollSpeed = 880.f;
	const float RollDuration = 0.45f;
	const float VaultDuration = 0.42f;
	const float MouseSensitivity = 0.9f;
	const float StickLookRate = 150.f;
}

AHFPlayerCharacter::AHFPlayerCharacter()
{
	TeamId = 0;
	MaxHealth = Health = 100.f;
	TracerColor = FLinearColor(1.f, 0.78f, 0.35f);

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->bUsePawnControlRotation = true;
	Boom->TargetArmLength = 280.f;
	Boom->SocketOffset = FVector(0.f, 70.f, 55.f);
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 14.f;
	Boom->ProbeSize = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(80.f);

	// Gritty grade: crushed saturation, heavy vignette, grain.
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.55f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.9f;
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(0.8f, 0.8f, 0.8f, 1.f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.3f;
}

void AHFPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	BuildBody(FLinearColor(0.13f, 0.18f, 0.24f), FLinearColor(0.05f, 0.055f, 0.06f), FLinearColor(1.f, 0.42f, 0.04f));

	FHFWeaponStats Carbine;
	Carbine.Name = TEXT("MK-9 RIVET CARBINE");
	Carbine.Damage = 14.f;
	Carbine.FireInterval = 0.095f;
	Carbine.MagSize = 40;
	Carbine.ReloadTime = 1.9f;
	Carbine.SpreadDeg = 1.6f;
	AddWeapon(Carbine, 280);

	FHFWeaponStats Breaker;
	Breaker.Name = TEXT("BREAKER 12");
	Breaker.Damage = 17.f;
	Breaker.Pellets = 9;
	Breaker.FireInterval = 0.85f;
	Breaker.MagSize = 6;
	Breaker.ReloadTime = 2.4f;
	Breaker.SpreadDeg = 5.5f;
	Breaker.Range = 2500.f;
	Breaker.FalloffStart = 450.f;
	Breaker.bAuto = false;
	AddWeapon(Breaker, 36);
}

void AHFPlayerCharacter::RefillAmmo()
{
	if (Weapons.Num() >= 2)
	{
		Weapons[0].Reserve = FMath::Max(Weapons[0].Reserve, 280);
		Weapons[1].Reserve = FMath::Max(Weapons[1].Reserve, 36);
	}
}

void AHFPlayerCharacter::OnDealtDamage(bool bKilled, bool bHeadshot)
{
	LastHitMarkerTime = GetWorld()->GetTimeSeconds();
	bLastHitKill = bKilled;
}

float AHFPlayerCharacter::GetSpreadMul() const
{
	if (bInCover && bCoverLow && !bAiming)
	{
		return 4.f; // blind fire over the top
	}
	float Mul = bAiming ? 0.35f : 1.f;
	if (GetVelocity().SizeSquared2D() > 150.f * 150.f)
	{
		Mul *= 1.6f;
	}
	return Mul;
}

void AHFPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// ---- look
	const float LookX = PC->GetInputAnalogKeyState(EKeys::MouseX) * MouseSensitivity
		+ PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX) * StickLookRate * DeltaSeconds;
	float LookY = PC->GetInputAnalogKeyState(EKeys::MouseY) * MouseSensitivity
		+ PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY) * StickLookRate * DeltaSeconds;
	if (bInvertLookY)
	{
		LookY = -LookY;
	}
	const float AimScale = bAiming ? 0.6f : 1.f;
	FRotator Control = PC->GetControlRotation();
	Control.Yaw += LookX * AimScale;
	Control.Pitch = FMath::Clamp(static_cast<float>(FRotator::NormalizeAxis(Control.Pitch)) + LookY * AimScale, -65.f, 65.f);
	Control.Roll = 0.f;
	PC->SetControlRotation(Control);

	UpdateCamera(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Regenerate after a few seconds out of the line of fire.
	if (Health < MaxHealth && Now - LastDamageTime > 4.f)
	{
		Health = FMath::Min(MaxHealth, Health + 35.f * DeltaSeconds);
	}

	if (bVaulting)
	{
		TickVault(DeltaSeconds);
		return;
	}

	// ---- input
	const float Fwd = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f)
		+ PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const float Rgt = (PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f)
		+ PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const FRotator YawRot(0.f, Control.Yaw, 0.f);
	const FVector CamFwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector CamRight = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const FVector WorldMove = (CamFwd * Fwd + CamRight * Rgt).GetClampedToMaxSize(1.f);

	const bool bAimHeld = PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > 0.3f;
	const bool bFireHeld = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.3f;
	const bool bSprintHeld = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick);
	const bool bCoverPressed = PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom);
	const bool bReloadPressed = PC->WasInputKeyJustPressed(EKeys::R) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left);
	const bool bSwapPressed = PC->WasInputKeyJustPressed(EKeys::Q) || PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)
		|| PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top);
	const bool bMeleePressed = PC->WasInputKeyJustPressed(EKeys::F) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right);

	// ---- cover / roll key
	if (bCoverPressed && !bRolling)
	{
		if (bInCover)
		{
			const bool bPushingIn = FVector::DotProduct(WorldMove, -CoverNormal) > 0.5f;
			if (!(bCoverLow && bPushingIn && TryVault()))
			{
				ExitCover();
			}
		}
		else
		{
			const FVector Dir = WorldMove.IsNearlyZero() ? CamFwd : WorldMove.GetSafeNormal();
			if (!TryEnterCover(Dir) && !WorldMove.IsNearlyZero())
			{
				StartRoll(Dir);
			}
		}
		if (bVaulting)
		{
			return;
		}
	}

	// ---- movement
	UCharacterMovementComponent* Move = GetCharacterMovement();
	bSprinting = false;
	if (bRolling)
	{
		bAiming = false;
		Move->Velocity = FVector(RollDir.X * RollSpeed, RollDir.Y * RollSpeed, Move->Velocity.Z);
		if (Now >= RollEndTime)
		{
			bRolling = false;
			bForceLowPose = false;
		}
	}
	else if (bInCover)
	{
		Move->MaxWalkSpeed = CoverSpeed;
		TickCover(DeltaSeconds, WorldMove, bAimHeld);
	}
	else
	{
		bAiming = bAimHeld;
		bSprinting = bSprintHeld && !bAiming && !bFireHeld && Fwd > 0.3f;
		Move->MaxWalkSpeed = bAiming ? AimSpeed : (bSprinting ? SprintSpeed : WalkSpeed);
		AddMovementInput(WorldMove);
	}

	// ---- facing
	if (!bInCover)
	{
		FRotator Target = GetActorRotation();
		if (bRolling)
		{
			Target = RollDir.Rotation();
		}
		else if (bAiming || Now - LastFireTime < 0.6f)
		{
			Target = YawRot;
		}
		else if (GetVelocity().SizeSquared2D() > 400.f)
		{
			Target = FRotator(0.f, GetVelocity().Rotation().Yaw, 0.f);
		}
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 14.f));
	}

	// ---- weapons
	if (!bRolling)
	{
		if (bSwapPressed && Weapons.Num() > 1)
		{
			Weapons[WeaponIdx].bReloading = false;
			WeaponIdx = (WeaponIdx + 1) % Weapons.Num();
		}
		if (bReloadPressed)
		{
			StartReload();
		}
		if (bMeleePressed && Now >= NextMeleeTime)
		{
			DoMelee();
		}
		HandleFire(PC, bFireHeld);
	}
	bFireWasHeld = bFireHeld;
}

// ------------------------------------------------------------------ camera

void AHFPlayerCharacter::UpdateCamera(float DeltaSeconds)
{
	float Arm = 280.f;
	float Fov = 80.f;
	FVector Offset(0.f, 70.f, 55.f);
	if (bAiming)
	{
		Arm = 135.f;
		Fov = 56.f;
		Offset = FVector(0.f, 58.f, 48.f);
	}
	else if (bSprinting)
	{
		Arm = 240.f;
		Fov = 90.f;
		Offset = FVector(0.f, 55.f, 20.f);
	}
	else if (bInCover)
	{
		Arm = 300.f;
		Offset = FVector(0.f, 75.f, 70.f);
	}

	// Camera shoulder follows the side being peeked from.
	const float WantShoulder = (bInCover && PeekSide != 0) ? static_cast<float>(PeekSide) * (FVector::DotProduct(CoverTangent(), Camera->GetRightVector()) >= 0.f ? 1.f : -1.f) : 1.f;
	ShoulderSign = FMath::FInterpTo(ShoulderSign, WantShoulder, DeltaSeconds, 8.f);
	Offset.Y *= ShoulderSign;

	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, Arm, DeltaSeconds, 10.f);
	Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, Offset, DeltaSeconds, 10.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, Fov, DeltaSeconds, 10.f));
}

// ------------------------------------------------------------------- cover

bool AHFPlayerCharacter::TryEnterCover(const FVector& Dir)
{
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Start(GetActorLocation().X, GetActorLocation().Y, FeetZ + 50.f);
	const FVector End = Start + Dir * 220.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HFCover), false, this);
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(18.f), Params))
	{
		return false;
	}
	UPrimitiveComponent* Comp = Hit.GetComponent();
	if (!Comp || !Comp->ComponentHasTag(AHFArena::CoverTag))
	{
		return false;
	}

	// All cover is axis-aligned; snap the normal so corner hits stay clean.
	FVector N = Hit.ImpactNormal;
	N.Z = 0.f;
	if (N.IsNearlyZero())
	{
		return false;
	}
	N = FMath::Abs(N.X) >= FMath::Abs(N.Y) ? FVector(FMath::Sign(N.X), 0.f, 0.f) : FVector(0.f, FMath::Sign(N.Y), 0.f);

	FVector Target = Hit.ImpactPoint + N * (Radius + 3.f);
	Target.Z = GetActorLocation().Z;
	SetActorLocation(Target, true);

	bInCover = true;
	bCoverLow = Comp->ComponentHasTag(AHFArena::LowCoverTag);
	CoverNormal = N;
	CoverComp = Comp;
	PeekSide = 0;
	PeekAlpha = 0.f;
	PeekApplied = FVector::ZeroVector;
	CoverFacingSign = FVector::DotProduct(Dir, CoverTangent()) >= 0.f ? 1.f : -1.f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	if (bCoverLow)
	{
		Crouch();
	}
	return true;
}

void AHFPlayerCharacter::ExitCover()
{
	bInCover = false;
	bAiming = false;
	PeekSide = 0;
	PeekAlpha = 0.f;
	PeekApplied = FVector::ZeroVector;
	UnCrouch();
}

bool AHFPlayerCharacter::HasCoverAt(float SideSign) const
{
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Anchor = GetActorLocation() - PeekApplied;
	const FVector Start = FVector(Anchor.X, Anchor.Y, FeetZ + 50.f) + CoverTangent() * SideSign * (Radius + 15.f);
	const FVector End = Start - CoverNormal * (Radius + 45.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HFCoverEdge), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}
	return Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(AHFArena::CoverTag);
}

void AHFPlayerCharacter::TickCover(float DeltaSeconds, const FVector& WorldMove, bool bAimHeld)
{
	const FVector T = CoverTangent();
	const float Along = FVector::DotProduct(WorldMove, T);
	const float Away = FVector::DotProduct(WorldMove, CoverNormal);
	const bool bSettled = PeekAlpha < 0.02f;

	if (bSettled)
	{
		if (!HasCoverAt(0.f) || (Away > 0.75f && !bAimHeld))
		{
			ExitCover();
			return;
		}
	}

	if (bCoverLow)
	{
		// Low cover: aiming stands you up over the top.
		bAiming = bAimHeld;
		if (bAiming && bIsCrouched)
		{
			UnCrouch();
		}
		else if (!bAiming && !bIsCrouched)
		{
			Crouch();
		}
	}
	else
	{
		// Tall cover: aiming only works from a corner, by leaning out.
		if (bAimHeld && PeekSide == 0 && bSettled)
		{
			const bool bEdgeR = !HasCoverAt(1.f);
			const bool bEdgeL = !HasCoverAt(-1.f);
			if (bEdgeR && bEdgeL)
			{
				PeekSide = CoverFacingSign >= 0.f ? 1 : -1;
			}
			else if (bEdgeR || bEdgeL)
			{
				PeekSide = bEdgeR ? 1 : -1;
			}
		}
		const bool bPeeking = bAimHeld && PeekSide != 0;
		PeekAlpha = FMath::FInterpConstantTo(PeekAlpha, bPeeking ? 1.f : 0.f, DeltaSeconds, 7.f);
		const float PeekDist = GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.f + 20.f;
		const FVector Desired = T * static_cast<float>(PeekSide) * PeekDist * PeekAlpha;
		AddActorWorldOffset(Desired - PeekApplied, true);
		PeekApplied = Desired;
		if (!bPeeking && PeekAlpha <= 0.f)
		{
			PeekSide = 0;
		}
		bAiming = bPeeking && PeekAlpha > 0.6f;
	}

	// Slide along the cover, stopping at its ends.
	if (!bAimHeld && PeekAlpha < 0.02f && FMath::Abs(Along) > 0.15f)
	{
		const float Sign = FMath::Sign(Along);
		CoverFacingSign = Sign;
		if (HasCoverAt(Sign))
		{
			AddMovementInput(T * Sign, FMath::Abs(Along));
		}
	}

	FRotator Target = (T * CoverFacingSign).Rotation();
	if (bAiming)
	{
		Target = FRotator(0.f, GetControlRotation().Yaw, 0.f);
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 14.f));
}

// ------------------------------------------------------------------- vault

bool AHFPlayerCharacter::TryVault()
{
	UPrimitiveComponent* Comp = CoverComp.Get();
	if (!Comp)
	{
		return false;
	}
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Extent = Comp->Bounds.BoxExtent;
	const float Thickness = 2.f * (FMath::Abs(CoverNormal.X) * Extent.X + FMath::Abs(CoverNormal.Y) * Extent.Y);

	const FVector StartFeet = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	const FVector EndFeet = StartFeet - CoverNormal * (Thickness + 2.f * (Radius + 3.f) + 12.f);

	// The landing spot must be clear.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HFVault), false, this);
	const FVector Probe = EndFeet + FVector(0.f, 0.f, 70.f);
	if (GetWorld()->OverlapBlockingTestByChannel(Probe, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, 60.f), Params))
	{
		return false;
	}

	bVaulting = true;
	bInCover = false;
	bAiming = false;
	VaultTime = 0.f;
	VaultStartFeet = StartFeet;
	VaultEndFeet = EndFeet;
	bForceLowPose = true;
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	SetActorRotation((-CoverNormal).Rotation());
	return true;
}

void AHFPlayerCharacter::TickVault(float DeltaSeconds)
{
	VaultTime += DeltaSeconds;
	const float A = FMath::Clamp(VaultTime / VaultDuration, 0.f, 1.f);
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Feet = FMath::Lerp(VaultStartFeet, VaultEndFeet, A);
	Feet.Z += FMath::Sin(A * PI) * 125.f;
	SetActorLocation(Feet + FVector(0.f, 0.f, HalfHeight), false);

	if (A >= 1.f)
	{
		bVaulting = false;
		bForceLowPose = false;
		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		UnCrouch();
	}
}

void AHFPlayerCharacter::StartRoll(const FVector& Dir)
{
	bRolling = true;
	bForceLowPose = true;
	RollDir = Dir.GetSafeNormal2D();
	RollEndTime = GetWorld()->GetTimeSeconds() + RollDuration;
}

// ------------------------------------------------------------------ combat

void AHFPlayerCharacter::HandleFire(APlayerController* PC, bool bFireHeld)
{
	const FHFWeapon* W = GetCurrentWeapon();
	if (!W || !bFireHeld || (!W->Stats.bAuto && bFireWasHeld))
	{
		return;
	}
	// Behind tall cover you have to lean out before you can shoot.
	if (bInCover && !bCoverLow && !bAiming)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector CamDir = CamRot.Vector();
	const float Skip = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(GetActorLocation() - CamLoc, CamDir)));
	const FVector TraceStart = CamLoc + CamDir * Skip;
	const FVector TraceEnd = CamLoc + CamDir * 12000.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HFAim), false, this);
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params) ? Hit.ImpactPoint : TraceEnd;

	const FRotator YawRot(0.f, CamRot.Yaw, 0.f);
	const FVector Fwd = YawRot.Vector();
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 42.f) + Fwd * 45.f + Right * 20.f * ShoulderSign;
	if (bInCover && bCoverLow && bIsCrouched)
	{
		// Blind fire: weapon held up over the lip of the cover.
		Muzzle = GetActorLocation() + FVector(0.f, 0.f, 78.f) - CoverNormal * 25.f;
	}

	if (FireShot(Muzzle, AimPoint, GetSpreadMul()))
	{
		LastFireTime = GetWorld()->GetTimeSeconds();
		// Recoil kick.
		FRotator Control = PC->GetControlRotation();
		Control.Pitch = FMath::Clamp(static_cast<float>(FRotator::NormalizeAxis(Control.Pitch)) + (W->Stats.Pellets > 1 ? 2.2f : 0.28f), -65.f, 65.f);
		Control.Yaw += FMath::FRandRange(-0.18f, 0.18f);
		PC->SetControlRotation(Control);
	}
}

void AHFPlayerCharacter::DoMelee()
{
	NextMeleeTime = GetWorld()->GetTimeSeconds() + 0.8f;
	LastFireTime = GetWorld()->GetTimeSeconds();

	const FVector Fwd = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	const FVector Center = GetActorLocation() + Fwd * 95.f;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HFMelee), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(100.f), Params);

	TSet<AActor*> Struck;
	for (const FOverlapResult& O : Overlaps)
	{
		AHFCharacterBase* Victim = Cast<AHFCharacterBase>(O.GetActor());
		if (Victim && !Victim->IsDead() && Victim->GetTeamId() != TeamId && !Struck.Contains(Victim))
		{
			Struck.Add(Victim);
			Victim->ReceiveHit(110.f, this, GetActorLocation());
			OnDealtDamage(Victim->IsDead(), false);
		}
	}
	if (!bInCover)
	{
		LaunchCharacter(Fwd * 500.f, true, false);
	}
}
