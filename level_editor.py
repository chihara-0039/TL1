import bpy
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
# シーン情報を出力するオペレーター
# =========================================================
class MYADDON_OT_export_scene(bpy.types.Operator):
    # Blender内部で使うID
    # メニューから呼ぶときは "myaddon.export_scene" と書く
    bl_idname = "myaddon.export_scene"

    # メニューに表示される名前
    bl_label = "シーン出力"

    # 説明文
    bl_description = "シーン情報をExportします"

    # Ctrl + Z の対象にする必要は薄いが、授業資料に合わせて登録系オペレーターとして扱う
    bl_options = {'REGISTER'}

    # 実行されたときに呼ばれる関数
    def execute(self, context):
        print("シーン情報をExportします")

        # シーン内の全オブジェクトを順番に調べる
        for obj in context.scene.objects:
            # オブジェクトの種類と名前を表示
            print(obj.type + " - " + obj.name)

            # ローカルトランスフォーム行列から
            # 平行移動、回転、スケールを取り出す
            trans, rot, scale = obj.matrix_local.decompose()

            # Quaternion回転をEuler角に変換
            rot = rot.to_euler()

            # ラジアンから度数法に変換
            rot.x = math.degrees(rot.x)
            rot.y = math.degrees(rot.y)
            rot.z = math.degrees(rot.z)

            # トランスフォーム情報を表示
            print("Trans(%f,%f,%f)" % (trans.x, trans.y, trans.z))
            print("Rot(%f,%f,%f)" % (rot.x, rot.y, rot.z))
            print("Scale(%f,%f,%f)" % (scale.x, scale.y, scale.z))

            # 親オブジェクトがある場合は親の名前も表示
            if obj.parent:
                print("Parent:" + obj.parent.name)

            # 次のオブジェクトと区切るための空行
            print()

        print("シーン情報をExportしました")

        # Blenderウィンドウ下部にメッセージ表示
        self.report({'INFO'}, "シーン情報をExportしました")

        return {'FINISHED'}


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