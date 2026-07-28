import os
import re

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # SkinnedObject.cpp
    content = content.replace('スキニングモチE', 'スキニングモデル')
    content = content.replace('E期化', '初期化')
    content = content.replace('冁E', '内部')
    
    # Object3dCommon
    content = content.replace('方向E決まってぁEがE源が無限遠にある照明、E', '方向は決まっているが光源が無限遠にある照明。')
    content = content.replace('平行E源', '平行光源')
    content = content.replace('光E色', '光の色')
    content = content.replace('然な昼光、E', '自然な昼光。')
    content = content.replace('降ってくる方吁E', '降ってくる方向')
    content = content.replace('侁E (0,-1,0) ↁE真上から降るE、E', '例: (0,-1,0) は真上から降る光。')
    content = content.replace('構造佁E', '構造体')
    
    content = content.replace('// 1. Transform', '// 1. Transform (トランスフォーム)')
    content = content.replace('// 2. Light', '// 2. Light (平行光源)')
    content = content.replace('// 3. Texture', '// 3. Texture (テクスチャ)')
    content = content.replace('// 4. ShadowMap', '// 4. ShadowMap (シャドウマップ)')
    content = content.replace('// 5. JointMatrices (SRV)', '// 5. JointMatrices (ジョイント行列バッファ)')

    # English to Japanese
    content = content.replace('// Bind Vertex Buffer and Draw', '// 頂点バッファをバインドして描画')
    content = content.replace('// Set Skinned Pipeline', '// スキニング用のパイプラインを設定')
    content = content.replace('// Restore normal pipeline state just in case', '// 念のため通常のパイプラインに戻す')
    content = content.replace('// Create joint buffer (always create at least 1 element to avoid null buffer crash in root signature)', '// ジョイントバッファを作成 (ルートシグネチャでのNULLバッファによるクラッシュを防ぐため、常に最低1要素を作成する)')
    content = content.replace('// Initialize dummy joint buffer with identity matrix', '// ダミーのジョイントバッファを単位行列で初期化')
    content = content.replace('// Update jointBuffer_', '// ジョイントバッファを更新')
    content = content.replace('// 0. Material', '// 0. マテリアル')

    # Remove totally broken lines
    content = re.sub(r'//.*.*', '', content)
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

for root, dirs, files in os.walk('Engine/Graphics'):
    for f in files:
        if f.endswith('.cpp') or f.endswith('.h'):
            fix_file(os.path.join(root, f))
