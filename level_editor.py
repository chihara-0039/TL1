import bpy

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
    # Blender内部で使うID
    # メニューから呼ぶときは "myaddon.stretch_vertex" と書く
    bl_idname = "myaddon.stretch_vertex"

    # メニューに表示される名前
    bl_label = "頂点を伸ばす"

    # 説明文
    bl_description = "頂点を引っ張って伸ばします"

    # Ctrl + Z で戻せるようにする
    bl_options = {'REGISTER', 'UNDO'}

    # 実行されたときに呼ばれる関数
    def execute(self, context):
        # 現在選択中のオブジェクトを取得
        obj = context.object

        # オブジェクトがない場合
        if obj is None:
            self.report({'WARNING'}, "オブジェクトが選択されていません")
            return {'CANCELLED'}

        # メッシュではない場合
        if obj.type != 'MESH':
            self.report({'WARNING'}, "メッシュオブジェクトを選択してください")
            return {'CANCELLED'}

        # 頂点が存在しない場合
        if len(obj.data.vertices) == 0:
            self.report({'WARNING'}, "頂点がありません")
            return {'CANCELLED'}

        # 0番目の頂点をX方向に伸ばす
        obj.data.vertices[0].co.x += 1.0

        print("頂点を伸ばしました。")

        return {'FINISHED'}


# =========================================================
# ICO球を作成するオペレーター
# =========================================================
class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    # Blender内部で使うID
    # メニューから呼ぶときは "myaddon.create_ico_sphere" と書く
    bl_idname = "myaddon.create_ico_sphere"

    # メニューに表示される名前
    bl_label = "ICO球を生成"

    # 説明文
    bl_description = "ICO球を生成します"

    # Ctrl + Z で戻せるようにする
    bl_options = {'REGISTER', 'UNDO'}

    # 実行されたときに呼ばれる関数
    def execute(self, context):
        # ICO球を追加
        bpy.ops.mesh.primitive_ico_sphere_add()

        print("ICO球を生成しました。")

        return {'FINISHED'}


# =========================================================
# トップバーに追加する自作メニュークラス
# =========================================================
class TOPBAR_MT_my_menu(bpy.types.Menu):
    # Blender内部で使うID
    bl_idname = "TOPBAR_MT_my_menu"

    # メニューに表示される名前
    bl_label = "MyMenu"

    # メニューの説明
    bl_description = "拡張メニュー by Taro Kamata"

    # メニューの中身を描画する関数
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
    TOPBAR_MT_my_menu,
)


# =========================================================
# アドオン有効化時に呼ばれる
# =========================================================
def register():
    # 自作クラスをBlenderに登録
    for cls in classes:
        bpy.utils.register_class(cls)

    # トップバーのメニューに自作メニューを追加
    bpy.types.TOPBAR_MT_editor_menus.append(draw_menu)

    print("レベルエディタが有効化されました。")


# =========================================================
# アドオン無効化時に呼ばれる
# =========================================================
def unregister():
    # トップバーのメニューから自作メニューを削除
    bpy.types.TOPBAR_MT_editor_menus.remove(draw_menu)

    # 自作クラスをBlenderから削除
    # 登録した順番と逆順で解除する
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    print("レベルエディタが無効化されました。")


# =========================================================
# Blenderのテキストエディタから直接実行した時用
# =========================================================
if __name__ == "__main__":
    register()