// ---------------------------------------------------------------------------
//  GameBox :: gfx.h — все 1-битные изображения (сгенерировано tools/gen_assets.py)
// ---------------------------------------------------------------------------
#pragma once
#include "../core/canvas.h"

// Логотип приставки 48x40
extern const Bmp bmpLogo;

// Баннеры игр 128x32
extern const Bmp bmpBannerFish;
extern const Bmp bmpBannerSpts;
extern const Bmp bmpBannerShot;
extern const Bmp bmpBannerRng;
extern const Bmp bmpBannerLife;

// Рыбалка: спрайты рыб (высота 13), лодки, иконки
extern const Bmp fishSprites[];
extern const uint8_t fishSpriteCount;
extern const Bmp boatSprites[];
extern const uint8_t boatSpriteCount;
extern const Bmp bmpFridge;
extern const Bmp bmpPotion;
extern const Bmp bmpRod;

// SPTS: зоны 39x26, навыки 42x37
extern const Bmp spsZoneSprites[];
extern const uint8_t spsZoneCount;
extern const Bmp spsSkillSprites[];
extern const uint8_t spsSkillCount;

// Buckshot: кадры интро 128x64 и сцены

extern const Bmp bmpIconCig;
extern const Bmp bmpIconMag;
extern const Bmp bmpIconBeer;
extern const Bmp bmpIconSaw;
extern const Bmp bmpIconCuff;
extern const Bmp bmpIconInv;
extern const Bmp bmpIconMed;
extern const Bmp bmpIconPhone;

// Иконки интерфейса 8x8/12x12
extern const Bmp bmpIconHeart;
extern const Bmp bmpIconShell;
extern const Bmp bmpIconLock;
extern const Bmp bmpIconStar;
