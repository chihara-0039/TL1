import bpy
import bpy_extras
import math
import gpu
import gpu_extras.batch
import copy
import mathutils

# Blenderに登録するアドオン情報
bl_info = {
    "name": "レベルエディタ",
    "author": "Taro Kamata",
    "version": (1, 0),
    "blender": (3, 3, 1),
    "location": "",
    "description": "レベルエディタ",
    "warning": "",
    "support": "TESTING",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Object",
}


# =========================================================
# 頂点を伸ばすオペレーター
# =========================================================
class MYADDON_OT_stretch_vertex(bpy.types.Operator):
    bl_idname = "myaddon.stretch_vertex"
    bl_label = "頂点を伸ばす"
    bl_description = "頂点を引っ張って伸ばします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object

        if obj is None:
            self.report({'WARNING'}, "オブジェクトが選択されていません")
            return {'CANCELLED'}

        if obj.type != 'MESH':
            self.report({'WARNING'}, "メッシュオブジェクトを選択してください")
            return {'CANCELLED'}

        if len(obj.data.vertices) == 0:
            self.report({'WARNING'}, "頂点がありません")
            return {'CANCELLED'}

        obj.data.vertices[0].co.x += 1.0

        print("頂点を伸ばしました。")

        return {'FINISHED'}


# =========================================================
# ICO球を作成するオペレーター
# =========================================================
class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.create_ico_sphere"
    bl_label = "ICO球を生成"
    bl_description = "ICO球を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()

        print("ICO球を生成しました。")

        return {'FINISHED'}


# =========================================================
# カスタムプロパティ「file_name」を追加するオペレーター
# =========================================================
class MYADDON_OT_add_filename(bpy.types.Operator):
    bl_idname = "myaddon.add_filename"
    bl_label = "FileName追加"
    bl_description = "選択中のオブジェクトにモデルファイル名用のカスタムプロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object

        if obj is None:
            self.report({'WARNING'}, "オブジェクトが選択されていません")
            return {'CANCELLED'}

        # すでに存在しているなら上書きしない
        if "file_name" not in obj:
            obj["file_name"] = ""

        print("カスタムプロパティ file_name を追加しました。")

        return {'FINISHED'}


# =========================================================
# カスタムプロパティ「collider」を追加するオペレーター
# =========================================================
class MYADDON_OT_add_collider(bpy.types.Operator):
    bl_idname = "myaddon.add_collider"
    bl_label = "コライダー追加"
    bl_description = "選択中のオブジェクトにBoxコライダー用のカスタムプロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object

        if obj is None:
            self.report({'WARNING'}, "オブジェクトが選択されていません")
            return {'CANCELLED'}

        # Boxコライダー用のカスタムプロパティを追加
        obj["collider"] = "BOX"
        obj["collider_center"] = mathutils.Vector((0.0, 0.0, 0.0))
        obj["collider_size"] = mathutils.Vector((2.0, 2.0, 2.0))

        print("Boxコライダー用カスタムプロパティを追加しました。")

        return {'FINISHED'}


# =========================================================
# カスタムプロパティ表示用パネル
# =========================================================
class OBJECT_PT_file_name(bpy.types.Panel):
    bl_idname = "OBJECT_PT_file_name"
    bl_label = "FileName"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        obj = context.object

        if obj is None:
            layout.label(text="オブジェクトが選択されていません")
            return

        # file_name が無い場合は追加ボタンを表示
        if "file_name" not in obj:
            layout.operator(MYADDON_OT_add_filename.bl_idname, text="FileName追加")
            return

        # file_name がある場合は編集欄を表示
        layout.prop(obj, '["file_name"]', text="FileName")


# =========================================================
# コライダー表示用パネル
# =========================================================
class OBJECT_PT_collider(bpy.types.Panel):
    bl_idname = "OBJECT_PT_collider"
    bl_label = "Collider"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        obj = context.object

        if obj is None:
            layout.label(text="オブジェクトが選択されていません")
            return

        if "collider" in obj:
            layout.prop(obj, '["collider"]', text="Type")
            layout.prop(obj, '["collider_center"]', text="Center")
            layout.prop(obj, '["collider_size"]', text="Size")
        else:
            layout.operator(MYADDON_OT_add_collider.bl_idname, text="コライダー追加")


# =========================================================
# コライダー描画クラス
# =========================================================
class DrawCollider:
    # 描画ハンドル
    handle = None

    @staticmethod
    def draw_collider():
        # 頂点データ
        vertices = {"pos": []}

        # インデックスデータ
        indices = []

        # 立方体の頂点となる8点のローカル座標
        offsets = [
            [-0.5, -0.5, -0.5],
            [+0.5, -0.5, -0.5],
            [-0.5, +0.5, -0.5],
            [+0.5, +0.5, -0.5],
            [-0.5, -0.5, +0.5],
            [+0.5, -0.5, +0.5],
            [-0.5, +0.5, +0.5],
            [+0.5, +0.5, +0.5],
        ]

        # シーン上の全オブジェクトを走査
        for obj in bpy.context.scene.objects:
            # collider カスタムプロパティが無いオブジェクトは描画しない
            if "collider" not in obj:
                continue

            # collider が BOX 以外なら描画しない
            if obj["collider"] != "BOX":
                continue

            # コライダー用プロパティを取得
            center = mathutils.Vector((0.0, 0.0, 0.0))
            size = mathutils.Vector((2.0, 2.0, 2.0))

            if "collider_center" in obj:
                center[0] = obj["collider_center"][0]
                center[1] = obj["collider_center"][1]
                center[2] = obj["collider_center"][2]

            if "collider_size" in obj:
                size[0] = obj["collider_size"][0]
                size[1] = obj["collider_size"][1]
                size[2] = obj["collider_size"][2]

            # このオブジェクトの頂点開始番号
            start = len(vertices["pos"])

            # Boxの8頂点を追加
            for offset in offsets:
                # コライダー中心点からのローカル座標を作る
                pos = copy.copy(center)
                pos[0] += offset[0] * size[0]
                pos[1] += offset[1] * size[1]
                pos[2] += offset[2] * size[2]

                # ローカル座標からワールド座標に変換
                pos = obj.matrix_world @ pos

                vertices["pos"].append(pos)

            # 前面
            indices.append([start + 0, start + 1])
            indices.append([start + 1, start + 3])
            indices.append([start + 3, start + 2])
            indices.append([start + 2, start + 0])

            # 奥面
            indices.append([start + 4, start + 5])
            indices.append([start + 5, start + 7])
            indices.append([start + 7, start + 6])
            indices.append([start + 6, start + 4])

            # 前面と奥面をつなぐ辺
            indices.append([start + 0, start + 4])
            indices.append([start + 1, start + 5])
            indices.append([start + 2, start + 6])
            indices.append([start + 3, start + 7])

        # 描画する頂点が無ければ終了
        if len(vertices["pos"]) == 0:
            return

        # ビルトインシェーダを取得
        shader = gpu.shader.from_builtin("UNIFORM_COLOR")

        # バッチを作成
        batch = gpu_extras.batch.batch_for_shader(
            shader,
            "LINES",
            vertices,
            indices=indices
        )

        # 色を設定して描画
        color = [0.5, 1.0, 1.0, 1.0]
        shader.bind()
        shader.uniform_float("color", color)
        batch.draw(shader)



# =========================================================
# シーン情報をファイルに出力するオペレーター
# =========================================================
class MYADDON_OT_export_scene(
    bpy.types.Operator,
    bpy_extras.io_utils.ExportHelper
):
    # Blender内部で使うID
    bl_idname = "myaddon.export_scene"

    # メニューに表示される名前
    bl_label = "シーン出力"

    # 説明文
    bl_description = "シーン情報をExportします"

    # ファイル選択UIで使う拡張子
    filename_ext = ".scene"

    # オペレーター実行時に呼ばれる
    def execute(self, context):
        print("シーン情報をExportします")

        # 実際のファイル出力処理
        self.export()

        print("シーン情報をExportしました")
        self.report({'INFO'}, "シーン情報をExportしました")

        return {'FINISHED'}

    # -----------------------------------------------------
    # コンソール表示とファイル出力を同時に行う関数
    # -----------------------------------------------------
    def write_and_print(self, file, text):
        print(text)
        file.write(text)
        file.write("\n")

    # -----------------------------------------------------
    # 実際にファイルを書き出す関数
    # -----------------------------------------------------
    def export(self):
        print("シーン情報出力開始... %r" % self.filepath)

        with open(self.filepath, "wt", encoding="utf-8") as file:
            # 最初にファイル識別用の文字列を書き込む
            self.write_and_print(file, "SCENE")

            # シーン内の全オブジェクトを調べる
            for obj in bpy.context.scene.objects:
                # 親があるオブジェクトは、親側の再帰処理で出力する
                if obj.parent:
                    continue

                # ルート直下のオブジェクトだけ再帰処理開始
                self.parse_scene_recursive(file, obj, 0)

    # -----------------------------------------------------
    # シーン解析用の再帰関数
    # -----------------------------------------------------
    def parse_scene_recursive(self, file, obj, level):
        # 深さに応じてタブを増やす
        indent = ""
        for i in range(level):
            indent += "\t"

        # オブジェクトの種類
        self.write_and_print(file, indent + obj.type)

        # ローカルトランスフォーム行列から
        # 平行移動、回転、スケールを取り出す
        trans, rot, scale = obj.matrix_local.decompose()

        # Quaternion回転をEuler角に変換
        rot = rot.to_euler()

        # ラジアンから度数法に変換
        rot.x = math.degrees(rot.x)
        rot.y = math.degrees(rot.y)
        rot.z = math.degrees(rot.z)

        # トランスフォーム情報を書き出す
        self.write_and_print(
            file,
            indent + "T %f %f %f" % (trans.x, trans.y, trans.z)
        )
        self.write_and_print(
            file,
            indent + "R %f %f %f" % (rot.x, rot.y, rot.z)
        )
        self.write_and_print(
            file,
            indent + "S %f %f %f" % (scale.x, scale.y, scale.z)
        )

        # カスタムプロパティ file_name がある場合だけ出力
        if "file_name" in obj:
            self.write_and_print(
                file,
                indent + "N %s" % obj["file_name"]
            )

        # カスタムプロパティ collider がある場合だけ出力
        if "collider" in obj:
            self.write_and_print(
                file,
                indent + "C %s" % obj["collider"]
            )
            self.write_and_print(
                file,
                indent + "CC %f %f %f" % (
                    obj["collider_center"][0],
                    obj["collider_center"][1],
                    obj["collider_center"][2]
                )
            )
            self.write_and_print(
                file,
                indent + "CS %f %f %f" % (
                    obj["collider_size"][0],
                    obj["collider_size"][1],
                    obj["collider_size"][2]
                )
            )

        # オブジェクトの終端
        self.write_and_print(file, indent + "END")

        # 子オブジェクトを再帰的に処理する
        for child in obj.children:
            self.parse_scene_recursive(file, child, level + 1)


# =========================================================
# トップバーに追加する自作メニュークラス
# =========================================================
class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "TOPBAR_MT_my_menu"
    bl_label = "MyMenu"
    bl_description = "拡張メニュー by Taro Kamata"

    def draw(self, context):
        # 「頂点を伸ばす」項目を追加
        self.layout.operator(
            MYADDON_OT_stretch_vertex.bl_idname,
            text=MYADDON_OT_stretch_vertex.bl_label
        )

        # 「ICO球を生成」項目を追加
        self.layout.operator(
            MYADDON_OT_create_ico_sphere.bl_idname,
            text=MYADDON_OT_create_ico_sphere.bl_label
        )

        # 「FileName追加」項目を追加
        self.layout.operator(
            MYADDON_OT_add_filename.bl_idname,
            text=MYADDON_OT_add_filename.bl_label
        )

        # 「コライダー追加」項目を追加
        self.layout.operator(
            MYADDON_OT_add_collider.bl_idname,
            text=MYADDON_OT_add_collider.bl_label
        )

        # 区切り線
        self.layout.separator()

        # 「シーン出力」項目を追加
        self.layout.operator(
            MYADDON_OT_export_scene.bl_idname,
            text=MYADDON_OT_export_scene.bl_label
        )


# =========================================================
# トップバーに自作メニューを追加するための関数
# =========================================================
def draw_menu(self, context):
    self.layout.menu(TOPBAR_MT_my_menu.bl_idname)


# =========================================================
# Blenderに登録するクラス一覧
# =========================================================
classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_add_filename,
    MYADDON_OT_add_collider,
    MYADDON_OT_export_scene,
    TOPBAR_MT_my_menu,
    OBJECT_PT_file_name,
    OBJECT_PT_collider,
)


# =========================================================
# アドオン有効化時に呼ばれる
# =========================================================
def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_editor_menus.append(draw_menu)
    
    # 3Dビューに描画関数を追加
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(
        DrawCollider.draw_collider,
        (),
        "WINDOW",
        "POST_VIEW"
    )

    print("レベルエディタが有効化されました。")


# =========================================================
# アドオン無効化時に呼ばれる
# =========================================================
def unregister():
    # 3Dビューから描画関数を削除
    if DrawCollider.handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, "WINDOW")
        DrawCollider.handle = None
        
    bpy.types.TOPBAR_MT_editor_menus.remove(draw_menu)

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    print("レベルエディタが無効化されました。")


# =========================================================
# Blenderのテキストエディタから直接実行した時用
# =========================================================
if __name__ == "__main__":
    register()
