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


# トップバーに追加する自作メニュークラス
class TOPBAR_MT_my_menu(bpy.types.Menu):
    # Blender内部で使うID
    bl_idname = "TOPBAR_MT_my_menu"

    # メニューに表示される名前
    bl_label = "MyMenu"

    # メニューの説明
    bl_description = "拡張メニュー by Taro Kamata"

    # メニューの中身を描画する関数
    def draw(self, context):
        # 「マニュアル」を開くオペレーターを追加
        self.layout.operator(
            "wm.url_open_preset",
            text="マニュアル",
            icon="HELP"
        )

        # 区切り線
        self.layout.separator()

        # 動作確認用に同じ項目を追加
        self.layout.operator(
            "wm.url_open_preset",
            text="チュートリアル",
            icon="HELP"
        )


# トップバーに自作メニューを追加するための関数
def draw_menu(self, context):
    self.layout.menu(TOPBAR_MT_my_menu.bl_idname)


# Blenderに登録するクラス一覧
classes = (
    TOPBAR_MT_my_menu,
)


# アドオン有効化時に呼ばれる
def register():
    # 自作クラスをBlenderに登録
    for cls in classes:
        bpy.utils.register_class(cls)

    # トップバーのメニューに自作メニューを追加
    bpy.types.TOPBAR_MT_editor_menus.append(draw_menu)

    print("レベルエディタが有効化されました。")


# アドオン無効化時に呼ばれる
def unregister():
    # トップバーのメニューから自作メニューを削除
    bpy.types.TOPBAR_MT_editor_menus.remove(draw_menu)

    # 自作クラスをBlenderから削除
    for cls in classes:
        bpy.utils.unregister_class(cls)

    print("レベルエディタが無効化されました。")


# Blenderのテキストエディタから直接実行した時用
if __name__ == "__main__":
    register()