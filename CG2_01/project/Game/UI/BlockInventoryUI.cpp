#include "BlockInventoryUI.h"
#include <cassert>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void BlockInventoryUI::Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, TextureManager* textureManager, BlockInventory* inventory) {
    assert(spriteCommon);
    assert(textureManager);
    assert(inventory);

    spriteCommon_ = spriteCommon;
    textureManager_ = textureManager;
    inventory_ = inventory;

    // 1. インベントリ背景画像のロード
    panelTextureHandle_ = textureManager_->LoadTexture("Resources/UI/inventory/inventory.png");
    
    const D3D12_RESOURCE_DESC& desc = textureManager_->GetResourceDesc(panelTextureHandle_);
    float imgW = static_cast<float>(desc.Width);
    float imgH = static_cast<float>(desc.Height);

    float scale = 720.0f / imgH;
    panelWidth_ = imgW * scale;
    panelHeight_ = 720.0f;

    tabWidth_ = panelWidth_ * (149.0f / 420.0f);

    panelSprite_ = std::make_unique<Sprite>();
    panelSprite_->Initialize(spriteCommon_, panelTextureHandle_);
    panelSprite_->SetSize({ panelWidth_, panelHeight_ });

    // 2. フォーカス枠の初期化 (white.pngを黄色カラーフィルタして使用)
    focusTextureHandle_ = textureManager_->LoadTexture("Resources/UI/inventory/white.png");
    focusSprite_ = std::make_unique<Sprite>();
    focusSprite_->Initialize(spriteCommon_, focusTextureHandle_);
    focusSprite_->SetColor({ 1.0f, 0.9f, 0.0f, 0.4f });

    // 3. ブロックボタンの設定 (2列の美しいグリッドレイアウト)
    float btnSize = 54.0f;
    Vector2 sizeVec = { btnSize, btnSize };

    auto addBtn = [&](BlockType type, int customId, float lx, float ly, const std::string& texPath) {
        BlockButton btn;
        btn.type = type;
        btn.customId = customId;
        btn.textureHandle = textureManager_->LoadTexture(texPath);
        btn.localPos = { lx, ly };
        btn.size = sizeVec;
        btn.sprite = std::make_unique<Sprite>();
        btn.sprite->Initialize(spriteCommon_, btn.textureHandle);
        btn.sprite->SetSize(sizeVec);

        // カスタムブロック（スロット1〜5）の場合、3x3シルエット表示用に9つのスプライトを生成
        if (customId >= 1 && customId <= 5) {
            uint32_t whiteTex = textureManager_->LoadTexture("Resources/UI/inventory/white.png");
            for (int i = 0; i < 9; ++i) {
                auto s = std::make_unique<Sprite>();
                s->Initialize(spriteCommon_, whiteTex);
                s->SetSize({ 10.0f, 10.0f }); // 1セルのサイズ: 10x10
                btn.silhouetteSprites.push_back(std::move(s));
            }
        }

        // 所持数ドット（最大8個）の生成
        {
            uint32_t whiteTex = textureManager_->LoadTexture("Resources/UI/inventory/white.png");
            for (int i = 0; i < 8; ++i) {
                auto s = std::make_unique<Sprite>();
                s->Initialize(spriteCommon_, whiteTex);
                s->SetSize({ 6.0f, 6.0f }); // ドットのサイズ: 6x6
                btn.countSprites.push_back(std::move(s));
            }
        }

        buttons_.push_back(std::move(btn));
    };

    // 通常ブロック (左列)
    addBtn(BlockType::Ground, 0, 174.0f, 130.0f, "Resources/Models/block/block.png");
    addBtn(BlockType::Wall,   0, 174.0f, 220.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Ladder, 0, 174.0f, 310.0f, "Resources/Models/ladder/ladder.png");

    // 新規特殊ブロック (空き座標を有効活用)
    addBtn(BlockType::IceBlock,       0, 249.0f, 130.0f, "Resources/Models/iceBlock/iceBlock.png");
    addBtn(BlockType::MovingFloor,    0, 249.0f, 490.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::CrumblingFloor, 0, 174.0f, 580.0f, "Resources/Models/CollapsedBlocks/CollapsedBlocks.png");

    // カスタムブロックスロット 1〜5 (空き位置に並べる)
    addBtn(BlockType::Wall,   1, 249.0f, 220.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   2, 249.0f, 310.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   3, 174.0f, 400.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   4, 249.0f, 400.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   5, 174.0f, 490.0f, "Resources/Models/wall/wall.png");

    // 4. アニメーション座標の初期化
    closedPos_ = { 1280.0f - arrowWidth_, 0.0f };
    openedPos_ = { 1280.0f - panelWidth_, 0.0f };
    currentPos_ = closedPos_;
    state_ = State::Closed;

    selectedBlockType_ = BlockType::Ground;
    selectedCustomId_ = 0;

    panelSprite_->SetPosition(currentPos_);
    panelSprite_->Update();
}

void BlockInventoryUI::Update(Input* input, WinApp* winApp, bool isGamePlayMode, const StageMap* stageMap) {
    stageMap_ = stageMap; // 毎フレーム受け取ったポインタを記録
    if (!isGamePlayMode) {
        state_ = State::Closed;
        currentPos_ = closedPos_;
        panelSprite_->SetPosition(currentPos_);
        panelSprite_->Update();
        return;
    }

    const auto& mouse = input->GetMouseState();
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float currentClientW = static_cast<float>(rect.right - rect.left);
    float currentClientH = static_cast<float>(rect.bottom - rect.top);

    if (currentClientW <= 0.0f || currentClientH <= 0.0f) {
        return;
    }

#ifdef NDEBUG
    float mouseX = static_cast<float>(mouse.posX) * (static_cast<float>(WinApp::kClientWidth) / currentClientW);
    float mouseY = static_cast<float>(mouse.posY) * (static_cast<float>(WinApp::kClientHeight) / currentClientH);
#else
    float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
    float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;

    float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
    float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

    float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY;
#endif

    bool clickTrigger = mouse.buttons[0] && !prevMouseLeft_;
    prevMouseLeft_ = mouse.buttons[0];

    // --- 1. アニメーション制御とイージング ---
    if (state_ == State::Opening) {
        currentPos_.x += (openedPos_.x - currentPos_.x) * 0.15f;
        if (std::abs(currentPos_.x - openedPos_.x) < 1.0f) {
            currentPos_ = openedPos_;
            state_ = State::Opened;
        }
    } else if (state_ == State::Closing) {
        currentPos_.x += (closedPos_.x - currentPos_.x) * 0.15f;
        if (std::abs(currentPos_.x - closedPos_.x) < 1.0f) {
            currentPos_ = closedPos_;
            state_ = State::Closed;
        }
    }

    panelSprite_->SetPosition(currentPos_);
    panelSprite_->Update();

    // --- 2. 取っ手クリックによる開閉制御 ---
    if (clickTrigger) {
        float arrowMinY = 0.0f; // 右上の黒い矢印があるY範囲（Y: 0〜130px）
        float arrowMaxY = 130.0f;
        bool hoverTab = (mouseX >= currentPos_.x && mouseX <= currentPos_.x + arrowWidth_ &&
                         mouseY >= arrowMinY && mouseY <= arrowMaxY);
        if (hoverTab) {
            ToggleOpen();
            return;
        }
    }

    // --- 3. ブロックボタンの更新とクリック判定 ---
    float startX = 174.0f;
    float startY = 130.0f;
    float colWidth = 75.0f;
    float rowHeight = 90.0f;
    int activeIndex = 0;

    for (auto& btn : buttons_) {
        // カスタムパーツプロパティの動的同期 (ベース挙動やカラーをリアルタイム適用)
        float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;
        if (btn.customId >= 1 && btn.customId <= 5 && stageMap) {
            const auto* part = stageMap->GetCustomPart(btn.customId);
            if (part) {
                btn.type = part->baseType;
                colorR = part->colorR;
                colorG = part->colorG;
                colorB = part->colorB;

                // ベース挙動に応じてテクスチャ変更
                if (btn.type == BlockType::Ladder) {
                    btn.textureHandle = textureManager_->LoadTexture("Resources/Models/ladder/ladder.png");
                } else {
                    btn.textureHandle = textureManager_->LoadTexture("Resources/Models/wall/wall.png");
                }
                btn.sprite->Initialize(spriteCommon_, btn.textureHandle);
                btn.sprite->SetSize(btn.size);
            }
        } else {
            // 通常の特殊ブロックタイプ用のカラー補正
            if (btn.type == BlockType::IceBlock) {
                colorR = 0.5f;
                colorG = 0.85f;
                colorB = 1.0f; // 美しいアイスブルー
            } else if (btn.type == BlockType::MovingFloor) {
                colorR = 0.9f;
                colorG = 0.65f;
                colorB = 0.4f; // インダストリアルオレンジ
            }
        }

        // 所持数が0個より多いもののみ有効（Groundブロックも含めて所持数依存）
        btn.isAvailable = (inventory_->GetBlockCount(btn.type, btn.customId) > 0);

        // 所持状態とカスタムカラーの乗算
        if (btn.isAvailable) {
            btn.sprite->SetColor({ colorR, colorG, colorB, 1.0f });
        } else {
            // 非表示になりますが、念のためカラー設定
            btn.sprite->SetColor({ colorR * 0.3f, colorG * 0.3f, colorB * 0.3f, 1.0f });
        }

        // 有効なブロックのみ動的レイアウトを適用
        if (btn.isAvailable) {
            int col = activeIndex % 2;
            int row = activeIndex / 2;
            btn.localPos = { startX + col * colWidth, startY + row * rowHeight };
            activeIndex++;
        }

        Vector2 screenPos = { currentPos_.x + btn.localPos.x, btn.localPos.y };
        btn.sprite->SetPosition(screenPos);
        btn.sprite->Update();

        // カスタムブロック（スロット1〜5）の3x3上面図シルエット用スプライトの更新
        if (btn.customId >= 1 && btn.customId <= 5 && btn.isAvailable && stageMap) {
            const auto* part = stageMap->GetCustomPart(btn.customId);
            if (part) {
                float gridStartX = btn.localPos.x + 10.0f;
                float gridStartY = btn.localPos.y + 10.0f;
                float cellSize = 12.0f;

                for (int z = 0; z < 3; ++z) {
                    for (int x = 0; x < 3; ++x) {
                        int idx = z * 3 + x;
                        if (idx >= 0 && idx < (int)btn.silhouetteSprites.size()) {
                            bool hasBlock = false;
                            for (int y = 0; y < 3; ++y) {
                                if (part->cells[y][z][x].type != BlockType::None) {
                                    hasBlock = true;
                                    break;
                                }
                            }

                            auto& s = btn.silhouetteSprites[idx];
                            Vector2 cellPos = { currentPos_.x + gridStartX + (float)x * cellSize, gridStartY + (float)z * cellSize };
                            s->SetPosition(cellPos);

                            if (hasBlock) {
                                s->SetColor({ part->colorR, part->colorG, part->colorB, 1.0f });
                            } else {
                                s->SetColor({ 0.25f, 0.25f, 0.25f, 0.5f }); // 空セルの薄暗い色
                            }
                            s->Update();
                        }
                    }
                }
            }
        }

        // 所持数ドットスプライトの座標とカラーを更新
        if (btn.isAvailable) {
            int count = inventory_->GetBlockCount(btn.type, btn.customId);
            int drawCount = (count > 8) ? 8 : count;

            float spacing = 6.0f; // ドットの間隔
            // ボタンの右下付近から左へ並べる
            float startX = btn.localPos.x + btn.size.x - 7.0f;
            float dotY = btn.localPos.y + btn.size.y - 7.0f;

            for (int i = 0; i < 8; ++i) {
                if (i < drawCount) {
                    float dotX = startX - i * spacing;
                    btn.countSprites[i]->SetPosition({ currentPos_.x + dotX, dotY });
                    btn.countSprites[i]->SetColor({ 0.2f, 1.0f, 0.2f, 1.0f }); // 明るい黄緑色
                    btn.countSprites[i]->Update();
                }
            }
        }

        // 開いているときのみクリック選択を受け付ける
        if (state_ == State::Opened && clickTrigger && btn.isAvailable) {
            if (mouseX >= screenPos.x && mouseX <= screenPos.x + btn.size.x &&
                mouseY >= screenPos.y && mouseY <= screenPos.y + btn.size.y) {
                
                uint32_t currentTick = GetTickCount();
                // 300ミリ秒以内の同一ボタンクリックをダブルクリックと判定
                if (currentTick - lastClickTick_ < 300 && lastClickedType_ == btn.type) {
                    selectedBlockType_ = btn.type;
                    selectedCustomId_ = btn.customId;
                    useRequested_ = true;
                    ToggleOpen();
                } else {
                    selectedBlockType_ = btn.type;
                    selectedCustomId_ = btn.customId;
                }

                lastClickTick_ = currentTick;
                lastClickedType_ = btn.type;
            }
        }
    }

    // 現在選択されている項目が非表示になった場合のバリデーション
    bool selectedAvailable = false;
    for (const auto& btn : buttons_) {
        if (btn.type == selectedBlockType_ && btn.customId == selectedCustomId_ && btn.isAvailable) {
            selectedAvailable = true;
            break;
        }
    }
    if (!selectedAvailable) {
        selectedBlockType_ = BlockType::None;
        selectedCustomId_ = 0;
        for (const auto& btn : buttons_) {
            if (btn.isAvailable) {
                selectedBlockType_ = btn.type;
                selectedCustomId_ = btn.customId;
                break;
            }
        }
    }

    // フォーカス枠の位置更新
    for (const auto& btn : buttons_) {
        if (btn.type == selectedBlockType_ && btn.customId == selectedCustomId_) {
            Vector2 screenPos = { currentPos_.x + btn.localPos.x - 4.0f, btn.localPos.y - 4.0f };
            focusSprite_->SetPosition(screenPos);
            focusSprite_->SetSize({ btn.size.x + 8.0f, btn.size.y + 8.0f });
            focusSprite_->Update();
            break;
        }
    }
}

void BlockInventoryUI::Draw() {
    spriteCommon_->PreDraw();

    panelSprite_->Draw();

    if (state_ != State::Closed) {
        if (selectedBlockType_ != BlockType::None) {
            focusSprite_->Draw();
        }

        for (const auto& btn : buttons_) {
            if (btn.isAvailable) {
                // カスタムパーツは単なる壁のテクスチャ画像の代わりに
                // 3x3上面図シルエットの9つのスプライトを描画します。
                if (btn.customId >= 1 && btn.customId <= 5) {
                    for (const auto& s : btn.silhouetteSprites) {
                        s->Draw();
                    }
                } else {
                    btn.sprite->Draw();
                }

                // 所持数ドットスプライトを描画
                int count = inventory_->GetBlockCount(btn.type, btn.customId);
                int drawCount = (count > 8) ? 8 : count;
                for (int i = 0; i < drawCount; ++i) {
                    btn.countSprites[i]->Draw();
                }
            }
        }
    }

#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (state_ != State::Closed) {
        // パネルに完全に被せる透明な ImGui ウィンドウを作成
        ImGui::SetNextWindowPos(ImVec2(currentPos_.x, currentPos_.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth_, panelHeight_), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        
        ImGui::Begin("InventoryOverlay", nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar | 
            ImGuiWindowFlags_NoBackground | 
            ImGuiWindowFlags_NoSavedSettings | 
            ImGuiWindowFlags_NoInputs);

        for (const auto& btn : buttons_) {
            if (!btn.isAvailable) continue; // 所持していないブロックはスキップして非表示

            // Ground (普通のブロック) は所持数無限
            if (btn.type == BlockType::Ground && btn.customId == 0) {
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y + btn.size.y + 2.0f));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.8f), "INF");
                
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y - 15.0f));
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 0.9f), "GROUND");
            } else {
                int count = inventory_->GetBlockCount(btn.type, btn.customId);
                
                // 所持数描画
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y + btn.size.y + 2.0f));
                if (count > 0) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "x%d", count);
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "x0");
                }

                // ラベル描画 (カスタムパーツならカスタム名に完全同期！)
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y - 15.0f));
                if (btn.customId >= 1 && btn.customId <= 5) {
                    std::string labelStr = "PART " + std::to_string(btn.customId);
                    if (stageMap_) {
                        const auto* part = stageMap_->GetCustomPart(btn.customId);
                        if (part && !part->name.empty()) {
                            labelStr = part->name;
                        }
                    }
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 0.9f), labelStr.c_str());

                    // --- 3x3 の形状シルエット（上面図）をカスタムカラーで描画 ---
                    if (stageMap_) {
                        const auto* part = stageMap_->GetCustomPart(btn.customId);
                        if (part) {
                            float gridStartX = btn.localPos.x + 9.0f; // ボタン(54x54)の中心に36x36グリッドを配置
                            float gridStartY = btn.localPos.y + 9.0f;
                            float cellSize = 12.0f;

                            ImDrawList* drawList = ImGui::GetWindowDrawList();
                            ImVec2 pMin = ImGui::GetWindowPos();

                            // グリッド全体の暗い背景
                            drawList->AddRectFilled(
                                ImVec2(pMin.x + gridStartX - 2.0f, pMin.y + gridStartY - 2.0f),
                                ImVec2(pMin.x + gridStartX + cellSize * 3 + 2.0f, pMin.y + gridStartY + cellSize * 3 + 2.0f),
                                IM_COL32(30, 30, 30, 220), 4.0f
                            );

                            for (int z = 0; z < 3; ++z) {
                                for (int x = 0; x < 3; ++x) {
                                    bool hasBlock = false;
                                    for (int y = 0; y < 3; ++y) {
                                        if (part->cells[y][z][x].type != BlockType::None) {
                                            hasBlock = true;
                                            break;
                                        }
                                    }

                                    ImVec2 cellMin = ImVec2(pMin.x + gridStartX + x * cellSize, pMin.y + gridStartY + z * cellSize);
                                    ImVec2 cellMax = ImVec2(cellMin.x + cellSize - 1.0f, cellMin.y + cellSize - 1.0f);

                                    if (hasBlock) {
                                        ImU32 cellColor = IM_COL32(
                                            static_cast<int>(part->colorR * 255),
                                            static_cast<int>(part->colorG * 255),
                                            static_cast<int>(part->colorB * 255),
                                            255
                                        );
                                        drawList->AddRectFilled(cellMin, cellMax, cellColor, 1.0f);
                                    } else {
                                        drawList->AddRect(cellMin, cellMax, IM_COL32(80, 80, 80, 100), 1.0f);
                                    }
                                }
                            }
                        }
                    }

                    // アセンブリであることを象徴する [ASSY] マークを描画
                    ImGui::SetCursorPos(ImVec2(btn.localPos.x + 3.0f, btn.localPos.y + 3.0f));
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.6f), "[ASSY]");
                } else {
                    if (btn.type == BlockType::Wall) {
                        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 0.9f), "WALL");
                    } else if (btn.type == BlockType::Ladder) {
                        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 0.9f), "LADDER");
                    } else if (btn.type == BlockType::IceBlock) {
                        ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 0.9f), "ICE");
                    } else if (btn.type == BlockType::MovingFloor) {
                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.5f, 0.9f), "MOVING");
                    } else if (btn.type == BlockType::CrumblingFloor) {
                        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.4f, 0.9f), "CRUMBLE");
                    }
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }
#endif
}

void BlockInventoryUI::ToggleOpen() {
    if (state_ == State::Closed || state_ == State::Closing) {
        state_ = State::Opening;
    } else if (state_ == State::Opened || state_ == State::Opening) {
        state_ = State::Closing;
    }
}

void BlockInventoryUI::Finalize() {
    panelSprite_.reset();
    focusSprite_.reset();
    for (auto& btn : buttons_) {
        btn.sprite.reset();
        btn.silhouetteSprites.clear();
        btn.countSprites.clear();
    }
    buttons_.clear();
}
