#pragma once
#include <vector>
#include <string>
#include <memory> // 追加
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"

class GameClearScene {
public:
    void Initialize(Object3dCommon* objCommon);
    void Update();
    void Draw();

    // 5/11 小林　ループするよう
    bool IsFinished() const { return isAllFinished_; }

    //5/19佐倉
    void SkipAnimation();
private:
    // 各文字データを管理する構造体
    struct Letter {
        // 1文字ごとの 3D リソースを所有
        std::unique_ptr<Object3d> object; // unique_ptr に変更
        std::unique_ptr<Model> model;     // unique_ptr に変更

        Vector3 position = { 0,0,0 };
        Vector3 scale = { 1,1,1 };

        bool isVisible = false;
        float bounceTime = 0.0f;
        float baseY = 0.0f;
    };

    // vector がクリアされる際、中の Letter ごとに unique_ptr が自動解放されます
    std::vector<Letter> letters_;

    // 借りているポインタ
    Object3dCommon* object3dCommon_ = nullptr;
    
    Camera camera_;
    Vector3 cameraPos_;
    Vector3 cameraRot_;

    float timer_ = 0.0f;
    bool isAllFinished_ = false;
    float finishTimer_ = 0.0f;

    std::string text_ = "COURSECLEAR";
};