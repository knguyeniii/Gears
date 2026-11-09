#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HFHUD.generated.h"

/** Canvas-drawn HUD: reticle, ammo, damage vignette, encounter banner. */
UCLASS()
class HOLDFAST_API AHFHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCentered(const FString& Text, const FLinearColor& Color, float Y, float Scale);
};
