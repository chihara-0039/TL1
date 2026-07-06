import bpy
import bpy_extras
import math

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
        # コンソールに表示
        print(text)

        # ファイルに書き込み
        # print() と違って file.write() は自動改行しないので \n を追加する
        file.write(text)
        file.write("\n")

    # -----------------------------------------------------
    # 実際にファイルを書き出す関数
    # -----------------------------------------------------
    def export(self):
        print("シーン情報出力開始... %r" % self.filepath)

        # ファイルをテキスト書き込みモードで開く
        # with を使うと、処理終了時に自動でファイルを閉じてくれる
        with open(self.filepath, "wt", encoding="utf-8") as file:
            # 最初にファイル識別用の文字列を書き込む
            self.write_and_print(file, "SCENE")

            # シーン内の全オブジェクトを調べる
            for obj in bpy.context.scene.objects:
                # 親があるオブジェクトはここでは処理しない
                # 親側の parse_scene_recursive() から子として処理する
                if obj.parent:
                    continue

                # ルート直下のオブジェクトだけ再帰処理開始
                self.parse_scene_recursive(file, obj, 0)

    # -----------------------------------------------------
    # シーン解析用の再帰関数
    # -----------------------------------------------------
    def parse_scene_recursive(self, file, obj, level):
        # 深さに応じてタブを増やす
        # level 0: 親なし
        # level 1: 子
        # level 2: 孫
        indent = ""
        for i in range(level):
            indent += "\t"

        # オブジェクトの種類と名前を書き出す
        self.write_and_print(file, indent + obj.type + " - " + obj.name)

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
            indent + "Trans(%f,%f,%f)" % (trans.x, trans.y, trans.z)
        )
        self.write_and_print(
            file,
            indent + "Rot(%f,%f,%f)" % (rot.x, rot.y, rot.z)
        )
        self.write_and_print(
            file,
            indent + "Scale(%f,%f,%f)" % (scale.x, scale.y, scale.z)
        )

        # 次のオブジェクトと区切るための空行
        self.write_and_print(file, "")

        # 子オブジェクトを再帰的に処理する
        # 子は level + 1 になるので、1段深くインデントされる
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
    MYADDON_OT_export_scene,
    TOPBAR_MT_my_menu,
)


# =========================================================
# アドオン有効化時に呼ばれる
# =========================================================
def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_editor_menus.append(draw_menu)

    print("レベルエディタが有効化されました。")


# =========================================================
# アドオン無効化時に呼ばれる
# =========================================================
def unregister():
    bpy.types.TOPBAR_MT_editor_menus.remove(draw_menu)

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    print("レベルエディタが無効化されました。")


# =========================================================
# Blenderのテキストエディタから直接実行した時用
# =========================================================
if __name__ == "__main__":
    register()