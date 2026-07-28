bl_info = {
    "name": "CG2 C++ Engine Level Editor",
    "author": "CG2_01",
    "version": (2, 1, 0),
    "blender": (3, 3, 0),
    "location": "3D View > Sidebar > CG2 Level",
    "description": "CG2_01のレベルJSONをBlenderで読み書きします",
    "category": "Object",
}

import json
import math
import os
from pathlib import Path

import bpy
import gpu
from bpy_extras.io_utils import ExportHelper, ImportHelper
from gpu_extras.batch import batch_for_shader
from mathutils import Vector


# -----------------------------------------------------------------------------
# JSON・パス共通処理
# -----------------------------------------------------------------------------

def _vector3(value, fallback):
    """JSON配列を安全にVectorへ変換する。"""
    if not isinstance(value, (list, tuple)) or len(value) < 3:
        return Vector(fallback)
    return Vector((float(value[0]), float(value[1]), float(value[2])))


def _resolve_model_path(level_path, file_name):
    """Resourcesから始まるエンジン相対パスを、実在する絶対パスへ解決する。"""
    if not file_name:
        return None

    source = Path(file_name)
    if source.is_absolute() and source.exists():
        return source

    level_directory = Path(level_path).resolve().parent
    candidates = [level_directory / source]
    candidates.extend(parent / source for parent in level_directory.parents)
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def _import_obj(model_path):
    """Blender 3系と4系の両方に対応してOBJを読み込む。"""
    before = set(bpy.context.scene.objects)
    if hasattr(bpy.ops.wm, "obj_import"):
        bpy.ops.wm.obj_import(filepath=str(model_path))
    else:
        bpy.ops.import_scene.obj(filepath=str(model_path))

    imported = [obj for obj in bpy.context.scene.objects if obj not in before]
    meshes = [obj for obj in imported if obj.type == 'MESH']
    if not meshes:
        return None

    # 一つのOBJが複数メッシュを持つ場合も、レベル上では一配置物として扱う。
    bpy.ops.object.select_all(action='DESELECT')
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    return bpy.context.view_layer.objects.active


def _clear_scene():
    """現在のシーンを削除してからレベルを復元する。"""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)


# -----------------------------------------------------------------------------
# JSON -> Blender
# -----------------------------------------------------------------------------

def _import_node(node, level_path, parent=None):
    """ノードを生成し、Transform・課題用プロパティ・親子関係を復元する。"""
    node_type = str(node.get("type", "EMPTY"))
    file_name = str(node.get("file_name", ""))
    obj = None

    if node_type == "MESH" and file_name:
        resolved = _resolve_model_path(level_path, file_name)
        if resolved is not None:
            obj = _import_obj(resolved)

    # モデルがないノードやSpawnPointも、編集可能なEmptyとして残す。
    if obj is None:
        obj = bpy.data.objects.new(str(node.get("name", node_type)), None)
        bpy.context.collection.objects.link(obj)
        obj.empty_display_type = 'CUBE'
        obj.empty_display_size = 0.35

    obj.name = str(node.get("name", obj.name))
    obj.parent = parent

    transform = node.get("transform", {})
    obj.location = _vector3(transform.get("translation"), (0.0, 0.0, 0.0))
    rotation_degrees = _vector3(transform.get("rotation"), (0.0, 0.0, 0.0))
    obj.rotation_mode = 'XYZ'
    obj.rotation_euler = tuple(math.radians(value) for value in rotation_degrees)
    obj.scale = _vector3(transform.get("scaling"), (1.0, 1.0, 1.0))

    obj["level_type"] = node_type
    if file_name:
        obj["file_name"] = file_name
    obj["disabled"] = bool(node.get("disabled", False))
    obj["spawn_enabled"] = False
    obj["spawn_type"] = "Player"

    collider = node.get("collider")
    if isinstance(collider, dict):
        obj["collider"] = str(collider.get("type", "BOX"))
        obj["collider_center"] = _vector3(collider.get("center"), (0.0, 0.0, 0.0))
        obj["collider_size"] = _vector3(collider.get("size"), (1.0, 1.0, 1.0))

    spawn = node.get("spawn_point")
    if isinstance(spawn, dict):
        obj["spawn_enabled"] = bool(spawn.get("enabled", True))
        obj["spawn_type"] = str(spawn.get("type", "Player"))

    for child in node.get("children", []):
        if isinstance(child, dict):
            _import_node(child, level_path, obj)
    return obj


class CG2_OT_import_level(bpy.types.Operator, ImportHelper):
    """エンジン互換JSONをBlenderシーンへ復元する。"""
    bl_idname = "cg2.import_level"
    bl_label = "Import Level JSON"
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = ".json"
    filter_glob: bpy.props.StringProperty(default="*.json", options={'HIDDEN'})
    clear_scene: bpy.props.BoolProperty(name="Clear Current Scene", default=True)

    def execute(self, context):
        try:
            with open(self.filepath, "r", encoding="utf-8") as source:
                root = json.load(source)
            if self.clear_scene:
                _clear_scene()
            for node in root.get("objects", []):
                if isinstance(node, dict):
                    _import_node(node, self.filepath)
            context.scene["cg2_level_name"] = str(root.get("name", Path(self.filepath).stem))
            context.scene["cg2_level_path"] = self.filepath
            self.report({'INFO'}, "CG2 level imported")
            return {'FINISHED'}
        except Exception as error:
            self.report({'ERROR'}, f"Import failed: {error}")
            return {'CANCELLED'}


# -----------------------------------------------------------------------------
# Blender -> JSON
# -----------------------------------------------------------------------------

def _serialize_node(obj):
    """BlenderオブジェクトをC++ LevelDataLoader互換ノードへ変換する。"""
    translation, rotation_quaternion, scaling = obj.matrix_local.decompose()
    rotation = rotation_quaternion.to_euler('XYZ')
    result = {
        "type": str(obj.get("level_type", "MESH" if obj.type == 'MESH' else obj.type)),
        "name": obj.name,
        "transform": {
            "translation": [translation.x, translation.y, translation.z],
            "rotation": [math.degrees(rotation.x), math.degrees(rotation.y), math.degrees(rotation.z)],
            "scaling": [scaling.x, scaling.y, scaling.z],
        },
        "children": [],
    }

    if "file_name" in obj:
        result["file_name"] = str(obj["file_name"])
    if bool(obj.get("disabled", False)):
        result["disabled"] = True
    if "collider" in obj:
        center = _vector3(obj.get("collider_center"), (0.0, 0.0, 0.0))
        size = _vector3(obj.get("collider_size"), (1.0, 1.0, 1.0))
        result["collider"] = {
            "type": str(obj.get("collider", "BOX")),
            "center": list(center),
            "size": list(size),
        }
    if bool(obj.get("spawn_enabled", False)):
        result["spawn_point"] = {
            "enabled": True,
            "type": str(obj.get("spawn_type", "Player")),
        }

    result["children"] = [_serialize_node(child) for child in obj.children]
    return result


class CG2_OT_export_level(bpy.types.Operator, ExportHelper):
    """現在のBlender配置をゲームで直接読めるJSONへ保存する。"""
    bl_idname = "cg2.export_level"
    bl_label = "Export Level JSON"
    filename_ext = ".json"
    filter_glob: bpy.props.StringProperty(default="*.json", options={'HIDDEN'})

    def invoke(self, context, event):
        saved_path = context.scene.get("cg2_level_path", "")
        if saved_path:
            self.filepath = saved_path
        return super().invoke(context, event)

    def execute(self, context):
        root = {
            "name": str(context.scene.get("cg2_level_name", Path(self.filepath).stem)),
            "version": 2,
            "objects": [_serialize_node(obj) for obj in context.scene.objects if obj.parent is None],
        }
        try:
            with open(self.filepath, "w", encoding="utf-8") as destination:
                json.dump(root, destination, ensure_ascii=False, indent=2)
            context.scene["cg2_level_path"] = self.filepath
            self.report({'INFO'}, "CG2 level exported")
            return {'FINISHED'}
        except Exception as error:
            self.report({'ERROR'}, f"Export failed: {error}")
            return {'CANCELLED'}


# -----------------------------------------------------------------------------
# プロパティ操作・可視化
# -----------------------------------------------------------------------------

class CG2_OT_add_collider(bpy.types.Operator):
    bl_idname = "cg2.add_collider"
    bl_label = "Add / Fit BOX Collider"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if obj is None:
            return {'CANCELLED'}
        obj["collider"] = "BOX"
        if obj.type == 'MESH':
            corners = [Vector(corner) for corner in obj.bound_box]
            minimum = Vector((min(v.x for v in corners), min(v.y for v in corners), min(v.z for v in corners)))
            maximum = Vector((max(v.x for v in corners), max(v.y for v in corners), max(v.z for v in corners)))
            obj["collider_center"] = (minimum + maximum) * 0.5
            obj["collider_size"] = maximum - minimum
        else:
            obj["collider_center"] = Vector((0.0, 0.0, 0.0))
            obj["collider_size"] = Vector((1.0, 1.0, 1.0))
        return {'FINISHED'}


class CG2_OT_initialize_object_properties(bpy.types.Operator):
    """Blender 5.2でも安全にCG2用プロパティを追加する。"""
    bl_idname = "cg2.initialize_object_properties"
    bl_label = "Initialize CG2 Properties"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if obj is None:
            return {'CANCELLED'}
        if "level_type" not in obj:
            obj["level_type"] = "MESH" if obj.type == 'MESH' else obj.type
        required = ("level_type", "disabled", "spawn_enabled", "spawn_type")
        if any(name not in obj for name in required):
            layout.operator(CG2_OT_initialize_object_properties.bl_idname, icon='ADD')
            layout.label(text="Initialize before editing CG2 settings.")
            return
        return {'FINISHED'}


class _ColliderDrawer:
    handle = None

    @staticmethod
    def draw():
        vertices = []
        indices = []
        offsets = [(-1,-1,-1),(1,-1,-1),(-1,1,-1),(1,1,-1),(-1,-1,1),(1,-1,1),(-1,1,1),(1,1,1)]
        edges = [(0,1),(1,3),(3,2),(2,0),(4,5),(5,7),(7,6),(6,4),(0,4),(1,5),(2,6),(3,7)]
        for obj in bpy.context.scene.objects:
            if str(obj.get("collider", "")) != "BOX":
                continue
            center = _vector3(obj.get("collider_center"), (0,0,0))
            size = _vector3(obj.get("collider_size"), (1,1,1))
            start = len(vertices)
            for offset in offsets:
                local = center + Vector((offset[0]*size.x*0.5, offset[1]*size.y*0.5, offset[2]*size.z*0.5))
                vertices.append(obj.matrix_world @ local)
            indices.extend((start+a, start+b) for a,b in edges)
        if not vertices:
            return
        shader = gpu.shader.from_builtin("UNIFORM_COLOR")
        batch = batch_for_shader(shader, 'LINES', {"pos": vertices}, indices=indices)
        shader.bind()
        shader.uniform_float("color", (0.1, 0.85, 1.0, 1.0))
        batch.draw(shader)


class CG2_PT_level_editor(bpy.types.Panel):
    bl_label = "CG2 Level Editor"
    bl_idname = "CG2_PT_level_editor"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'CG2 Level'

    def draw(self, context):
        layout = self.layout
        layout.operator(CG2_OT_import_level.bl_idname, icon='IMPORT')
        layout.operator(CG2_OT_export_level.bl_idname, icon='EXPORT')
        layout.separator()
        obj = context.object
        if obj is None:
            layout.label(text="Select an object")
            return
        # Blender側で新規作成したオブジェクトでも、パネルを安全に操作できる初期値を用意する。
        if "disabled" not in obj:
            obj["disabled"] = False
        if "spawn_enabled" not in obj:
            obj["spawn_enabled"] = False
        if "spawn_type" not in obj:
            obj["spawn_type"] = "Player"
        if "file_name" in obj:
            layout.prop(obj, '["file_name"]', text="Model")
        layout.prop(obj, '["disabled"]', text="Disabled")
        layout.operator(CG2_OT_add_collider.bl_idname)
        if "collider" in obj:
            layout.prop(obj, '["collider"]', text="Collider")
            layout.prop(obj, '["collider_center"]', text="Center")
            layout.prop(obj, '["collider_size"]', text="Size")
        layout.prop(obj, '["spawn_enabled"]', text="Spawn Point")
        if bool(obj.get("spawn_enabled", False)):
            layout.prop(obj, '["spawn_type"]', text="Spawn Type")


classes = (
    CG2_OT_import_level,
    CG2_OT_export_level,
    CG2_OT_add_collider,
    CG2_OT_initialize_object_properties,
    CG2_PT_level_editor,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    _ColliderDrawer.handle = bpy.types.SpaceView3D.draw_handler_add(
        _ColliderDrawer.draw, (), 'WINDOW', 'POST_VIEW')


def unregister():
    if _ColliderDrawer.handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_ColliderDrawer.handle, 'WINDOW')
        _ColliderDrawer.handle = None
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
