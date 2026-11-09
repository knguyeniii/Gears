#include "HFHUD.h"
#include "HFGameMode.h"
#include "HFPlayerCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void AHFHUD::DrawCentered(const FString& Text, const FLinearColor& Color, float Y, float Scale)
{
	UFont* Font = GEngine->GetLargeFont();
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.7f), (Canvas->SizeX - W) * 0.5f + 2.f, Y + 2.f, Font, Scale);
	DrawText(Text, Color, (Canvas->SizeX - W) * 0.5f, Y, Font, Scale);
}

void AHFHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float UI = FMath::Max(1.f, H / 1080.f);
	const float Now = GetWorld()->GetTimeSeconds();
	UFont* Font = GEngine->GetLargeFont();
	const FLinearColor Amber(1.f, 0.62f, 0.15f);
	const FLinearColor Pale(0.9f, 0.9f, 0.85f);

	const AHFPlayerCharacter* P = Cast<AHFPlayerCharacter>(GetOwningPawn());
	const AHFGameMode* GM = GetWorld()->GetAuthGameMode<AHFGameMode>();

	if (P)
	{
		// Damage vignette instead of a health bar: the screen edges close in red.
		const float Hurt = 1.f - FMath::Clamp(P->GetHealth() / P->GetMaxHealth(), 0.f, 1.f);
		const float Flash = FMath::Clamp(1.f - (Now - P->GetLastDamageTime()) / 0.35f, 0.f, 1.f);
		const float A = FMath::Clamp(Hurt * 0.75f + Flash * 0.25f, 0.f, 0.9f);
		if (A > 0.01f)
		{
			const int32 Bands = 8;
			for (int32 i = 0; i < Bands; ++i)
			{
				const float T = W * 0.022f;
				const FLinearColor C(0.6f, 0.f, 0.f, A * (1.f - static_cast<float>(i) / Bands) * 0.5f);
				DrawRect(C, 0.f, i * T, W, T);
				DrawRect(C, 0.f, H - (i + 1) * T, W, T);
				DrawRect(C, i * T, 0.f, T, H);
				DrawRect(C, W - (i + 1) * T, 0.f, T, H);
			}
		}

		if (!P->IsDead())
		{
			// Reticle opens with spread; hidden while sprinting.
			const float CX = W * 0.5f;
			const float CY = H * 0.5f;
			if (!P->IsSprinting())
			{
				const float Gap = (5.f + 9.f * P->GetSpreadMul()) * UI;
				const float Len = 9.f * UI;
				const FLinearColor RC = P->IsAiming() ? Pale : FLinearColor(0.9f, 0.9f, 0.85f, 0.6f);
				DrawLine(CX - Gap - Len, CY, CX - Gap, CY, RC, 2.f);
				DrawLine(CX + Gap, CY, CX + Gap + Len, CY, RC, 2.f);
				DrawLine(CX, CY - Gap - Len, CX, CY - Gap, RC, 2.f);
				DrawLine(CX, CY + Gap, CX, CY + Gap + Len, RC, 2.f);
			}
			const float HitAge = Now - P->GetLastHitMarkerTime();
			if (HitAge < 0.18f)
			{
				const FLinearColor HC = P->WasLastHitKill() ? FLinearColor(1.f, 0.15f, 0.1f) : FLinearColor::White;
				const float S = 12.f * UI;
				const float G = 5.f * UI;
				DrawLine(CX - S, CY - S, CX - G, CY - G, HC, 2.f);
				DrawLine(CX + S, CY - S, CX + G, CY - G, HC, 2.f);
				DrawLine(CX - S, CY + S, CX - G, CY + G, HC, 2.f);
				DrawLine(CX + S, CY + S, CX + G, CY + G, HC, 2.f);
			}

			if (const FHFWeapon* Wpn = P->GetCurrentWeapon())
			{
				const float X = W - 330.f * UI;
				const float Y = H - 120.f * UI;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 16.f * UI, Y - 12.f * UI, 320.f * UI, 96.f * UI);
				DrawRect(Amber, X - 16.f * UI, Y - 12.f * UI, 4.f * UI, 96.f * UI);
				DrawText(Wpn->Stats.Name, Pale, X, Y, Font, 1.1f * UI);
				const FString Ammo = Wpn->bReloading ? FString(TEXT("RELOADING")) : FString::Printf(TEXT("%d / %d"), Wpn->Ammo, Wpn->Reserve);
				const FLinearColor AC = (Wpn->bReloading || Wpn->Ammo <= Wpn->Stats.MagSize / 5) ? FLinearColor(1.f, 0.25f, 0.15f) : Amber;
				DrawText(Ammo, AC, X, Y + 30.f * UI, Font, 2.f * UI);
			}

			if (P->IsInCover())
			{
				DrawCentered(TEXT("IN COVER"), FLinearColor(1.f, 0.62f, 0.15f, 0.8f), H - 70.f * UI, 1.f * UI);
			}
		}
	}

	if (GM)
	{
		DrawText(FString::Printf(TEXT("BREACH %d / %d"), GM->GetWaveNumber(), GM->GetNumWaves()), Amber, 40.f * UI, 36.f * UI, Font, 1.4f * UI);
		DrawText(FString::Printf(TEXT("HOSTILES  %d"), GM->GetEnemiesRemaining()), Pale, 40.f * UI, 70.f * UI, Font, 1.1f * UI);

		if (Now < GM->GetBannerUntil() && !GM->GetBanner().IsEmpty())
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0.f, H * 0.2f - 14.f * UI, W, 84.f * UI);
			DrawCentered(GM->GetBanner(), GM->IsDefeat() ? FLinearColor(1.f, 0.2f, 0.12f) : Amber, H * 0.2f, 3.f * UI);
		}
	}

	if (Now < 14.f)
	{
		const float A = FMath::Clamp(14.f - Now, 0.f, 1.f);
		DrawCentered(TEXT("WASD move   SHIFT sprint   SPACE cover / vault / roll   RMB aim   LMB fire   R reload   Q swap   F melee"),
			FLinearColor(0.9f, 0.9f, 0.85f, A), H - 40.f * UI, 1.f * UI);
	}
}
