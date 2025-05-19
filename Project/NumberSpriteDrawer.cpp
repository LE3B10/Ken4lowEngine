#include "NumberSpriteDrawer.h"
#include <string>
#include <TextureManager.h>

void NumberSpriteDrawer::Initialize(const std::string& texturePath, float digitWidth, float digitHeight)
{
    digitWidth_ = digitWidth;
    digitHeight_ = digitHeight;

    TextureManager::GetInstance()->LoadTexture(texturePath);

    for (int i = 0; i < 10; ++i)
    {
        digitSprites_[i] = std::make_unique<Sprite>();
        digitSprites_[i]->Initialize(texturePath);

        int row = i / 5; // 0 or 1
        int col = i % 5;

        Vector2 uvPos = {
            static_cast<float>(col) * digitWidth_,
            static_cast<float>(row) * digitHeight_
        };
        Vector2 size = { digitWidth_, digitHeight_ };

        digitSprites_[i]->SetTextureLeftTop(uvPos);
        digitSprites_[i]->SetTextureSize(size);
        digitSprites_[i]->SetSize({ 48.0f, 48.0f }); // 表示サイズを小さく調整（50→32）
    }
}

void NumberSpriteDrawer::DrawNumber(int value, const Vector2& position)
{
    std::string numberStr = std::to_string(value);
    float x = position.x;

    for (char c : numberStr)
    {
        int digit = c - '0';
        if (digit < 0 || digit > 9) continue;

        // 各スプライトの複製を作成して個別に描画（同一スプライト再使用の不具合対策）
        std::unique_ptr<Sprite> tempSprite = std::make_unique<Sprite>(*digitSprites_[digit]);
        tempSprite->SetPosition({ x, position.y });
        tempSprite->Update();
        tempSprite->Draw();

        x += digitSprites_[digit]->GetSize().x;
    }
}
