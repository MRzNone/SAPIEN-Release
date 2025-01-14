import sapien.core as sapien
from PIL import Image, ImageColor
from sapien.core import Pose
import numpy as np
import open3d
import transforms3d

sim = sapien.Engine()
renderer = sapien.OptifuserRenderer()
sim.set_renderer(renderer)
scene = sim.create_scene()
scene.set_timestep(1 / 60)

scene.set_ambient_light([0.5, 0.5, 0.5])
scene.set_shadow_light([0, 1, -1], [0.5, 0.5, 0.5])
scene.add_point_light([1, 2, 2], [1, 1, 1])
scene.add_point_light([1, -2, 2], [1, 1, 1])
scene.add_point_light([-1, 0, 1], [1, 1, 1])

actor_builder = scene.create_actor_builder()
mesh_dir = "/home/sim/money.ply"
mesh_pos = np.random.rand(3)
actor_builder.add_visual_from_file(mesh_dir)
mesh = actor_builder.build_static()
mesh.set_pose(Pose(mesh_pos))

near, far = 0.1, 100
camera_mount_actor = scene.create_actor_builder().build(is_kinematic=True)
camera = scene.add_mounted_camera('first_camera', camera_mount_actor, Pose(), 640, 480, np.deg2rad(35),
                                  np.deg2rad(35), near, far)

pos = np.array([3, -2, 3])
forward = -pos / np.linalg.norm(pos)
left = np.cross([0, 0, 1], forward)
left = left / np.linalg.norm(left)
up = np.cross(forward, left)
mat44 = np.eye(4)
mat44[:3, :3] = np.linalg.inv(np.array([forward, left, up]))
mat44[:3, 3] = pos
camera_mount_actor.set_pose(Pose.from_transformation_matrix(mat44))

scene.step()
scene.update_render()
camera.take_picture()

rgba = camera.get_color_rgba()
rgba = (rgba * 255).clip(0, 255).astype('uint8')
rgba = Image.fromarray(rgba)
rgba.save('color.png')

camera_matrix = camera.get_camera_matrix()[:3, :3]
opengl_depth = camera.get_depth()
y, x = np.where(opengl_depth < 1)
z = near * far / (far + opengl_depth * (near - far))
permutation = np.array([[0, 0, 1], [-1, 0, 0], [0, -1, 0]])
points = (permutation @ np.dot(np.linalg.inv(camera_matrix),
                               np.stack([x, y, np.ones_like(x)] * z[y, x], 0))).T

# Transform from camera coordinate to world coordinate
points = np.concatenate([points, np.ones([points.shape[0], 1])], axis=1)
points = (mat44 @ points.T).T[:, :3]

cloud = open3d.geometry.PointCloud()
cloud.points = open3d.utility.Vector3dVector(points)
cloud.colors = open3d.utility.Vector3dVector(np.ones([points.shape[0], 3]) * np.array([[0, 0, 0.5]]))

open3d.io.write_point_cloud('cloud.pcd', cloud)

mesh_open3d = open3d.io.read_triangle_mesh(mesh_dir)
mesh_pose = np.eye(4)
mesh_pose[:3, 3] = mesh_pos
mesh_open3d.transform(mesh_pose)
frame = open3d.geometry.TriangleMesh.create_coordinate_frame(size=0.5, origin=mesh_pos)
open3d.visualization.draw_geometries([cloud, frame, mesh_open3d])
